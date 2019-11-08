//
//  AtariST.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "AtariST.hpp"

#include "../CRTMachine.hpp"
#include "../KeyboardMachine.hpp"
#include "../MouseMachine.hpp"
#include "../MediaTarget.hpp"
#include "../../Activity/Source.hpp"

//#define LOG_TRACE
#include "../../Processors/68000/68000.hpp"

#include "../../Components/AY38910/AY38910.hpp"
#include "../../Components/68901/MFP68901.hpp"
#include "../../Components/6850/6850.hpp"

#include "DMAController.hpp"
#include "IntelligentKeyboard.hpp"
#include "Video.hpp"

#include "../../ClockReceiver/JustInTime.hpp"
#include "../../ClockReceiver/ForceInline.hpp"

#include "../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"
#include "../../Outputs/Log.hpp"

#include "../Utility/MemoryPacker.hpp"
#include "../Utility/MemoryFuzzer.hpp"

namespace Atari {
namespace ST {

const int CLOCK_RATE = 8021247;

using Target = Analyser::Static::Target;
class ConcreteMachine:
	public Atari::ST::Machine,
	public CPU::MC68000::BusHandler,
	public CRTMachine::Machine,
	public ClockingHint::Observer,
	public Motorola::ACIA::ACIA::InterruptDelegate,
	public Motorola::MFP68901::MFP68901::InterruptDelegate,
	public DMAController::Delegate,
	public MouseMachine::Machine,
	public KeyboardMachine::MappedMachine,
	public Activity::Source,
	public MediaTarget::Machine,
	public GI::AY38910::PortHandler {
	public:
		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			mc68000_(*this),
			keyboard_acia_(Cycles(500000)),
			midi_acia_(Cycles(500000)),
			ay_(audio_queue_),
			speaker_(ay_),
			ikbd_(keyboard_acia_->transmit, keyboard_acia_->receive) {
			set_clock_rate(CLOCK_RATE);
			speaker_.set_input_rate(CLOCK_RATE / 4);

			ram_.resize(512 * 512);	// i.e. 512kb
			video_->set_ram(ram_.data(), ram_.size());
			Memory::Fuzz(ram_);

			std::vector<ROMMachine::ROM> rom_descriptions = {
				{"AtariST", "the TOS ROM", "tos100.img", 192*1024, 0x1a586c64}
			};
			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			Memory::PackBigEndian16(*roms[0], rom_);

			// Set up basic memory map.
			memory_map_[0] = BusDevice::MostlyRAM;
			int c = 1;
			for(; c < 0x08; ++c) memory_map_[c] = BusDevice::RAM;
			for(; c < 0xff; ++c) memory_map_[c] = BusDevice::Unassigned;

			const bool is_early_tos = true;
			if(is_early_tos) {
				for(c = 0xfc; c < 0xff; ++c) memory_map_[c] = BusDevice::ROM;
			} else {
				for(c = 0xe0; c < 0xe4; ++c) memory_map_[c] = BusDevice::ROM;
			}

			memory_map_[0xfa] = memory_map_[0xfb] = BusDevice::Cartridge;
			memory_map_[0xff] = BusDevice::IO;

			midi_acia_->set_interrupt_delegate(this);
			keyboard_acia_->set_interrupt_delegate(this);

			midi_acia_->set_clocking_hint_observer(this);
			keyboard_acia_->set_clocking_hint_observer(this);
			ikbd_.set_clocking_hint_observer(this);
			mfp_->set_clocking_hint_observer(this);
			dma_->set_clocking_hint_observer(this);

			mfp_->set_interrupt_delegate(this);
			dma_->set_delegate(this);
			ay_.set_port_handler(this);

			set_gpip_input();

			// Insert any supplied media.
			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_queue_.flush();
		}

		// MARK: CRTMachine::Machine
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			video_->set_scan_target(scan_target);
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &speaker_;
		}

		void run_for(const Cycles cycles) final {
			// Give the keyboard an opportunity to consume any events.
			if(!keyboard_needs_clock_) {
				ikbd_.run_for(HalfCycles(0));
			}

			mc68000_.run_for(cycles);
		}

		// MARK: MC68000::BusHandler
		using Microcycle = CPU::MC68000::Microcycle;
		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			// Just in case the last cycle was an interrupt acknowledge or bus error. TODO: find a better solution?
			mc68000_.set_is_peripheral_address(false);
			mc68000_.set_bus_error(false);

			// Advance time.
			advance_time(cycle.length);

			// A null cycle leaves nothing else to do.
			if(!(cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress))) return HalfCycles(0);

			// An interrupt acknowledge, perhaps?
			if(cycle.operation & Microcycle::InterruptAcknowledge) {
				// Current implementation: everything other than 6 (i.e. the MFP is autovectored.
				if((cycle.word_address()&7) != 6) {
					mc68000_.set_is_peripheral_address(true);
					return HalfCycles(0);
				} else {
					if(cycle.operation & Microcycle::SelectByte) {
						const int interrupt = mfp_->acknowledge_interrupt();
						if(interrupt != Motorola::MFP68901::MFP68901::NoAcknowledgement) {
							cycle.value->halves.low = uint8_t(interrupt);
						} else {
							// TODO: this should take a while. Find out how long.
							mc68000_.set_bus_error(true);
						}
					}
					return HalfCycles(0);
				}
			}

			// If this is a new strobing of the address signal, test for bus error and pre-DTack delay.
			HalfCycles delay(0);
			if(cycle.operation & Microcycle::NewAddress) {
				// DTack will be implicit; work out how long until that should be,
				// and apply bus error constraints.
				const int i_phase = bus_phase_.as<int>() & 7;
				if(i_phase < 4) {
					delay = HalfCycles(4 - i_phase);
					advance_time(delay);
				}

				// TODO: presumably test is if(after declared memory size and (not supervisor or before hardware space)) bus_error?
			}

			auto address = cycle.word_address();
//			if(cycle.data_select_active()) printf("%c %06x\n", (cycle.operation & Microcycle::Read) ? 'r' : 'w', *cycle.address & 0xffffff);
			uint16_t *memory;
			switch(memory_map_[address >> 15]) {
				case BusDevice::MostlyRAM:
					if(address < 4) {
						memory = rom_.data();
						break;
					}
				case BusDevice::RAM:
					memory = ram_.data();
					address &= ram_.size() - 1;
					// TODO: align with the next access window.
				break;

				case BusDevice::ROM:
					memory = rom_.data();
					address %= rom_.size();
				break;

				case BusDevice::Unassigned:
					// TODO: figure out the rules about bus errors.
				case BusDevice::Cartridge:
					/*
						TOS 1.0 appears to attempt to read from the catridge before it has setup
						the bus error vector. Therefore I assume no bus error flows.
					*/
					switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
						default: break;
						case Microcycle::SelectWord | Microcycle::Read:
							*cycle.value = 0xffff;
						break;
						case Microcycle::SelectByte | Microcycle::Read:
							cycle.value->halves.low = 0xff;
						break;
					}
				return delay;

				case BusDevice::IO:
					switch(address) {
						default:
//							assert(false);

						case 0x7fc000:
							/* Memory controller configuration:
									b0, b1: bank 1
									b2, b3: bank 0

									00 = 128k
									01 = 512k
									10 = 2mb
									11 = reserved
							*/
						break;

						case 0x7fc400:	/* PSG: write to select register, read to read register. */
						case 0x7fc401:	/* PSG: write to write register. */
							if(!cycle.data_select_active()) return delay;

							advance_time(HalfCycles(2));
							update_audio();

							if(cycle.operation & Microcycle::Read) {
								ay_.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BC2 | GI::AY38910::BC1));
								cycle.set_value8_high(ay_.get_data_output());
								ay_.set_control_lines(GI::AY38910::ControlLines(0));
							} else {
								if(address == 0x7fc400) {
									ay_.set_control_lines(GI::AY38910::BC1);
								} else {
									ay_.set_control_lines(GI::AY38910::ControlLines(GI::AY38910::BC2 | GI::AY38910::BDIR));
								}
								ay_.set_data_input(cycle.value8_high());
								ay_.set_control_lines(GI::AY38910::ControlLines(0));
							}
						return delay + HalfCycles(2);

						// The MFP block:
						case 0x7ffd00:	case 0x7ffd01:	case 0x7ffd02:	case 0x7ffd03:
						case 0x7ffd04:	case 0x7ffd05:	case 0x7ffd06:	case 0x7ffd07:
						case 0x7ffd08:	case 0x7ffd09:	case 0x7ffd0a:	case 0x7ffd0b:
						case 0x7ffd0c:	case 0x7ffd0d:	case 0x7ffd0e:	case 0x7ffd0f:
						case 0x7ffd10:	case 0x7ffd11:	case 0x7ffd12:	case 0x7ffd13:
						case 0x7ffd14:	case 0x7ffd15:	case 0x7ffd16:	case 0x7ffd17:
						case 0x7ffd18:	case 0x7ffd19:	case 0x7ffd1a:	case 0x7ffd1b:
						case 0x7ffd1c:	case 0x7ffd1d:	case 0x7ffd1e:	case 0x7ffd1f:
							if(!cycle.data_select_active()) return delay;

							if(cycle.operation & Microcycle::Read) {
								cycle.set_value8_low(mfp_->read(int(address)));
							} else {
								mfp_->write(int(address), cycle.value8_low());
							}
						break;

						// Video controls.
						case 0x7fc100:	case 0x7fc101:	case 0x7fc102:	case 0x7fc103:
						case 0x7fc104:	case 0x7fc105:	case 0x7fc106:	case 0x7fc107:
						case 0x7fc108:	case 0x7fc109:	case 0x7fc10a:	case 0x7fc10b:
						case 0x7fc10c:	case 0x7fc10d:	case 0x7fc10e:	case 0x7fc10f:
						case 0x7fc110:	case 0x7fc111:	case 0x7fc112:	case 0x7fc113:
						case 0x7fc114:	case 0x7fc115:	case 0x7fc116:	case 0x7fc117:
						case 0x7fc118:	case 0x7fc119:	case 0x7fc11a:	case 0x7fc11b:
						case 0x7fc11c:	case 0x7fc11d:	case 0x7fc11e:	case 0x7fc11f:
						case 0x7fc120:	case 0x7fc121:	case 0x7fc122:	case 0x7fc123:
						case 0x7fc124:	case 0x7fc125:	case 0x7fc126:	case 0x7fc127:
						case 0x7fc128:	case 0x7fc129:	case 0x7fc12a:	case 0x7fc12b:
						case 0x7fc12c:	case 0x7fc12d:	case 0x7fc12e:	case 0x7fc12f:
						case 0x7fc130:	case 0x7fc131:
							if(!cycle.data_select_active()) return delay;

							if(cycle.operation & Microcycle::Read) {
								cycle.set_value16(video_->read(int(address)));
							} else {
								video_->write(int(address), cycle.value16());
							}
						break;

						// ACIAs.
						case 0x7ffe00:	case 0x7ffe01:	case 0x7ffe02:	case 0x7ffe03: {
							// Set VPA.
							mc68000_.set_is_peripheral_address(!cycle.data_select_active());
							if(!cycle.data_select_active()) return delay;

							const auto acia_ = (address < 0x7ffe02) ? &keyboard_acia_ : &midi_acia_;
							if(cycle.operation & Microcycle::Read) {
								cycle.set_value8_high((*acia_)->read(int(address)));
							} else {
								(*acia_)->write(int(address), cycle.value8_high());
							}
						} break;

						// DMA.
						case 0x7fc302:	case 0x7fc303:	case 0x7fc304:	case 0x7fc305:	case 0x7fc306:
							if(!cycle.data_select_active()) return delay;

							if(cycle.operation & Microcycle::Read) {
								cycle.set_value16(dma_->read(int(address)));
							} else {
								dma_->write(int(address), cycle.value16());
							}
						break;
					}
				return HalfCycles(0);
			}

			// If control has fallen through to here, the access is either a read from ROM, or a read or write to RAM.
			switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
				default:
				break;

				case Microcycle::SelectWord | Microcycle::Read:
					cycle.value->full = memory[address];
				break;
				case Microcycle::SelectByte | Microcycle::Read:
					cycle.value->halves.low = uint8_t(memory[address] >> cycle.byte_shift());
				break;
				case Microcycle::SelectWord:
					memory[address] = cycle.value->full;
				break;
				case Microcycle::SelectByte:
					memory[address] = uint16_t(
						(cycle.value->halves.low << cycle.byte_shift()) |
						(memory[address] & cycle.untouched_byte_mask())
					);
				break;
			}

			return HalfCycles(0);
		}

		void flush() {
			dma_.flush();
			mfp_.flush();
			keyboard_acia_.flush();
			midi_acia_.flush();
			video_.flush();
			update_audio();
			audio_queue_.perform();
		}

	private:
		forceinline void advance_time(HalfCycles length) {
			// Advance the relevant counters.
			cycles_since_audio_update_ += length;
			mfp_ += length;
			dma_ += length;
			keyboard_acia_ += length;
			midi_acia_ += length;
			bus_phase_ += length;

			// Don't even count time for the keyboard unless it has requested it.
			if(keyboard_needs_clock_) {
				cycles_since_ikbd_update_ += length;
				ikbd_.run_for(cycles_since_ikbd_update_.divide(HalfCycles(512)));
			}

			// Flush anything that needs real-time updating.
			if(!may_defer_acias_) {
				keyboard_acia_.flush();
				midi_acia_.flush();
			}

			if(mfp_is_realtime_) {
				mfp_.flush();
			}

			if(dma_is_realtime_) {
				dma_.flush();
			}

			// Update the video output, checking whether a sequence point has been hit.
			while(length >= cycles_until_video_event_) {
				length -= cycles_until_video_event_;
				video_ += cycles_until_video_event_;
				cycles_until_video_event_ = video_->get_next_sequence_point();

				mfp_->set_timer_event_input(1, video_->display_enabled());
				update_interrupt_input();
			}
			cycles_until_video_event_ -= length;
			video_ += length;
		}

		void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_audio_update_.divide_cycles(Cycles(4)));
		}

		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;
		HalfCycles bus_phase_;

		JustInTimeActor<Video> video_;
		HalfCycles cycles_until_video_event_;

		// The MFP runs at 819200/2673749ths of the CPU clock rate.
		JustInTimeActor<Motorola::MFP68901::MFP68901, 819200, 2673749> mfp_;
		JustInTimeActor<Motorola::ACIA::ACIA, 16> keyboard_acia_;
		JustInTimeActor<Motorola::ACIA::ACIA, 16> midi_acia_;

		Concurrency::DeferringAsyncTaskQueue audio_queue_;
		GI::AY38910::AY38910 ay_;
		Outputs::Speaker::LowpassSpeaker<GI::AY38910::AY38910> speaker_;
		HalfCycles cycles_since_audio_update_;

		JustInTimeActor<DMAController> dma_;

		HalfCycles cycles_since_ikbd_update_;
		IntelligentKeyboard ikbd_;

		std::vector<uint16_t> ram_;
		std::vector<uint16_t> rom_;

		enum class BusDevice {
			MostlyRAM, RAM, ROM, Cartridge, IO, Unassigned
		};
		BusDevice memory_map_[256];

		// MARK: - Clocking Management.
		bool may_defer_acias_ = true;
		bool keyboard_needs_clock_ = false;
		bool mfp_is_realtime_ = false;
		bool dma_is_realtime_ = false;
		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) final {
			// This is being called by one of the components; avoid any time flushing here as that's
			// already dealt with (and, just to be absolutely sure, to avoid recursive mania).
			may_defer_acias_ =
				(keyboard_acia_.last_valid()->preferred_clocking() != ClockingHint::Preference::RealTime) &&
				(midi_acia_.last_valid()->preferred_clocking() != ClockingHint::Preference::RealTime);
			keyboard_needs_clock_ = ikbd_.preferred_clocking() != ClockingHint::Preference::None;
			mfp_is_realtime_ = mfp_.last_valid()->preferred_clocking() == ClockingHint::Preference::RealTime;
			dma_is_realtime_ = dma_.last_valid()->preferred_clocking() == ClockingHint::Preference::RealTime;
		}

		// MARK: - GPIP input.
		void acia6850_did_change_interrupt_status(Motorola::ACIA::ACIA *) final {
			set_gpip_input();
		}
		void dma_controller_did_change_output(DMAController *) final {
			set_gpip_input();

			// Filty hack, here! Should: set the 68000's bus request line. But until
			// that's implemented, just offers magical zero-cost DMA insertion and
			// extrication.
			if(dma_->get_bus_request_line()) {
				dma_->bus_grant(ram_.data(), ram_.size());
			}
		}
		void set_gpip_input() {
			/*
				Atari ST GPIP bits:

					GPIP 7: monochrome monitor detect
					GPIP 6: RS-232 ring indicator
					GPIP 5: FD/HD interrupt
					GPIP 4: keyboard/MIDI interrupt
					GPIP 3: unused
					GPIP 2: RS-232 clear to send
					GPIP 1: RS-232 carrier detect
					GPIP 0: centronics busy
			*/
			mfp_->set_port_input(
				0x80 |	// b7: Monochrome monitor detect (1 = is monochrome).
				0x40 |	// b6: RS-232 ring indicator.
				(dma_->get_interrupt_line() ? 0x00 : 0x20) |	// b5: FD/HS interrupt (0 = interrupt requested).
				((keyboard_acia_->get_interrupt_line() || midi_acia_->get_interrupt_line()) ? 0x00 : 0x10) |	// b4: Keyboard/MIDI interrupt (0 = interrupt requested).
				0x08 |	// b3: Unused
				0x04 |	// b2: RS-232 clear to send.
				0x02 |	// b1 : RS-232 carrier detect.
				0x00	// b0: Centronics busy (1 = busy).
			);
		}

		// MARK - MFP input.
		void mfp68901_did_change_interrupt_status(Motorola::MFP68901::MFP68901 *mfp) final {
			update_interrupt_input();
		}

		void update_interrupt_input() {
			if(mfp_->get_interrupt_line()) {
				mc68000_.set_interrupt_level(6);
			} else if(video_->vsync()) {
				mc68000_.set_interrupt_level(4);
			} else if(video_->hblank()) {
				mc68000_.set_interrupt_level(2);
			} else {
				mc68000_.set_interrupt_level(0);
			}
		}

		// MARK: - MouseMachine
		Inputs::Mouse &get_mouse() final {
			return ikbd_;
		}

		// MARK: - KeyboardMachine
		void set_key_state(uint16_t key, bool is_pressed) final {
			ikbd_.set_key_state(Key(key), is_pressed);
		}

		IntelligentKeyboard::KeyboardMapper keyboard_mapper_;
		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		// MARK: - AYPortHandler
		void set_port_output(bool port_b, uint8_t value) final {
			if(port_b) {
				// TODO: ?
			} else {
				/*
					TODO: Port A:
						b7: reserved
						b6: "freely usable output (monitor jack)"
						b5: centronics strobe
						b4: RS-232 DTR output
						b3: RS-232 RTS output
						b2: select floppy drive 1
						b1: select floppy drive 0
						b0: "page choice signal for double-sided floppy drive"
				*/
				dma_->set_floppy_drive_selection(!(value & 2), !(value & 4), !(value & 1));
			}
		}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &media) final {
			size_t c = 0;
			for(const auto &disk: media.disks) {
				dma_->set_floppy_disk(disk, c);
				++c;
				if(c == 2) break;
			}
			return true;
		}

		// MARK: - Activity Source
		void set_activity_observer(Activity::Observer *observer) override {
			dma_->set_activity_observer(observer);
		}
};

}
}

using namespace Atari::ST;

Machine *Machine::AtariST(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(*target, rom_fetcher);
}

Machine::~Machine() {}
