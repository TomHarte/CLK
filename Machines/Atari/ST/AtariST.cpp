//
//  AtariST.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "AtariST.hpp"

#include "../../MachineTypes.hpp"
#include "../../../Activity/Source.hpp"

//#define LOG_TRACE
//bool should_log = false;
#include "../../../Processors/68000Mk2/68000Mk2.hpp"

#include "../../../Components/AY38910/AY38910.hpp"
#include "../../../Components/68901/MFP68901.hpp"
#include "../../../Components/6850/6850.hpp"

#include "DMAController.hpp"
#include "IntelligentKeyboard.hpp"
#include "Video.hpp"

#include "../../../ClockReceiver/JustInTime.hpp"
#include "../../../ClockReceiver/ForceInline.hpp"
#include "../../../Configurable/StandardOptions.hpp"

#include "../../../Outputs/Speaker/Implementation/LowpassSpeaker.hpp"

#define LOG_PREFIX "[ST] "
#include "../../../Outputs/Log.hpp"

#include "../../Utility/MemoryPacker.hpp"
#include "../../Utility/MemoryFuzzer.hpp"

#include "../../../Analyser/Static/AtariST/Target.hpp"

namespace Atari {
namespace ST {

constexpr int CLOCK_RATE = 8021247;

using Target = Analyser::Static::AtariST::Target;
class ConcreteMachine:
	public Atari::ST::Machine,
	public CPU::MC68000Mk2::BusHandler,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public MachineTypes::AudioProducer,
	public MachineTypes::MouseMachine,
	public MachineTypes::JoystickMachine,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public ClockingHint::Observer,
	public Motorola::ACIA::ACIA::InterruptDelegate,
	public Motorola::MFP68901::MFP68901::InterruptDelegate,
	public DMAController::Delegate,
	public Activity::Source,
	public GI::AY38910::PortHandler,
	public Configurable::Device,
	public Video::RangeObserver {
	public:
		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			mc68000_(*this),
			keyboard_acia_(Cycles(500000)),
			midi_acia_(Cycles(500000)),
			ay_(GI::AY38910::Personality::YM2149F, audio_queue_),
			speaker_(ay_),
			ikbd_(keyboard_acia_->transmit, keyboard_acia_->receive) {
			set_clock_rate(CLOCK_RATE);
			speaker_.set_input_rate(float(CLOCK_RATE) / 4.0f);

			switch(target.memory_size) {
				default:
				case Target::MemorySize::FiveHundredAndTwelveKilobytes:
					ram_.resize(512 * 1024);
				break;
				case Target::MemorySize::OneMegabyte:
					ram_.resize(1024 * 1024);
				break;
				case Target::MemorySize::FourMegabytes:
					ram_.resize(4 * 1024 * 1024);
				break;
			}
			Memory::Fuzz(ram_);

			video_->set_ram(reinterpret_cast<uint16_t *>(ram_.data()), ram_.size());

			constexpr ROM::Name rom_name = ROM::Name::AtariSTTOS100;
			ROM::Request request(rom_name);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}
			Memory::PackBigEndian16(roms.find(rom_name)->second, rom_);

			// Set up basic memory map.
			memory_map_[0] = BusDevice::MostlyRAM;
			int c = 1;
			for(; c < int(ram_.size() >> 16); ++c) memory_map_[c] = BusDevice::RAM;
			for(; c < 0x40; ++c) memory_map_[c] = BusDevice::Floating;
			for(; c < 0xff; ++c) memory_map_[c] = BusDevice::Unassigned;

			const bool is_early_tos = true;
			if(is_early_tos) {
				rom_start_ = 0xfc0000;
				for(c = 0xfc; c < 0xff; ++c) memory_map_[c] = BusDevice::ROM;
			} else {
				rom_start_ = 0xe00000;
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

			video_->set_range_observer(this);

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

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return video_->get_scaled_scan_status();
		}

		void set_display_type(Outputs::Display::DisplayType display_type) final {
			video_->set_display_type(display_type);
		}

		Outputs::Display::DisplayType get_display_type() const final {
			return video_->get_display_type();
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
		template <typename Microcycle> HalfCycles perform_bus_operation(const Microcycle &cycle, int is_supervisor) {
			// Just in case the last cycle was an interrupt acknowledge or bus error. TODO: find a better solution?
			mc68000_.set_is_peripheral_address(false);
			mc68000_.set_bus_error(false);

			// Advance time.
			advance_time(cycle.length);

			// Check for assertion of reset.
			if(cycle.operation & Microcycle::Reset) {
				LOG("Unhandled Reset");
			}

			// A null cycle leaves nothing else to do.
			if(!(cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress))) return HalfCycles(0);

			// An interrupt acknowledge, perhaps?
			if(cycle.operation & Microcycle::InterruptAcknowledge) {
				// Current implementation: everything other than 6 (i.e. the MFP) is autovectored.
				const int interrupt_level = cycle.word_address()&7;
				if(interrupt_level != 6) {
					video_interrupts_pending_ &= ~interrupt_level;
					update_interrupt_input();
					mc68000_.set_is_peripheral_address(true);
					return HalfCycles(0);
				} else {
					if(cycle.operation & Microcycle::SelectByte) {
						const int interrupt = mfp_->acknowledge_interrupt();
						if(interrupt != Motorola::MFP68901::MFP68901::NoAcknowledgement) {
							cycle.value->b = uint8_t(interrupt);
						} else {
							// TODO: this should take a while. Find out how long.
							mc68000_.set_bus_error(true);
						}
					}
					return HalfCycles(0);
				}
			}

			auto address = cycle.host_endian_byte_address();

			// If this is a new strobing of the address signal, test for bus error and pre-DTack delay.
			HalfCycles delay(0);
			if(cycle.operation & Microcycle::NewAddress) {
				// Bus error test.
				if(
					// Anything unassigned should generate a bus error.
					(memory_map_[address >> 16] == BusDevice::Unassigned) ||

					// Bus errors also apply to unprivileged access to the first 0x800 bytes, or the IO area.
					(!is_supervisor && (address < 0x800 || memory_map_[address >> 16] == BusDevice::IO))
				) {
					mc68000_.set_bus_error(true);
					return delay;	// TODO: there should be an extra delay here.
				}

				// DTack delay rule: if accessing RAM or the shifter, align with the two cycles next available
				// for the CPU to access that side of the bus.
				if(address < ram_.size() || (address == 0xff8260)) {
					// DTack will be implicit; work out how long until that should be,
					// and apply bus error constraints.
					const int i_phase = bus_phase_.as<int>() & 7;
					if(i_phase < 4) {
						delay = HalfCycles(4 - i_phase);
						advance_time(delay);
					}
				}
			}

			uint8_t *memory = nullptr;
			switch(memory_map_[address >> 16]) {
				default:
				case BusDevice::MostlyRAM:
					if(address < 8) {
						memory = rom_.data();
						break;
					}
					[[fallthrough]];
				case BusDevice::RAM:
					memory = ram_.data();
				break;

				case BusDevice::ROM:
					memory = rom_.data();
					address -= rom_start_;
				break;

				case BusDevice::Floating:
					// TODO: provide vapour reads here. But: will these always be of the last video fetch?
				case BusDevice::Unassigned:
				case BusDevice::Cartridge:
					/*
						TOS 1.0 appears to attempt to read from the catridge before it has setup
						the bus error vector. Therefore I assume no bus error flows.
					*/
					switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
						default: break;
						case Microcycle::SelectWord | Microcycle::Read:
							cycle.value->w = 0xffff;
						break;
						case Microcycle::SelectByte | Microcycle::Read:
							cycle.value->b = 0xff;
						break;
					}
				return delay;

				case BusDevice::IO:
					switch(address & 0xfffe) {	// TODO: surely it's going to be even less precise than this?
						default:
//							assert(false);

						case 0x8000:
							/* Memory controller configuration:
									b0, b1: bank 1
									b2, b3: bank 0

									00 = 128k
									01 = 512k
									10 = 2mb
									11 = reserved
							*/
						break;

						// Video controls.
						case 0x8200:	case 0x8202:	case 0x8204:	case 0x8206:
						case 0x8208:	case 0x820a:	case 0x820c:	case 0x820e:
						case 0x8210:	case 0x8212:	case 0x8214:	case 0x8216:
						case 0x8218:	case 0x821a:	case 0x821c:	case 0x821e:
						case 0x8220:	case 0x8222:	case 0x8224:	case 0x8226:
						case 0x8228:	case 0x822a:	case 0x822c:	case 0x822e:
						case 0x8230:	case 0x8232:	case 0x8234:	case 0x8236:
						case 0x8238:	case 0x823a:	case 0x823c:	case 0x823e:
						case 0x8240:	case 0x8242:	case 0x8244:	case 0x8246:
						case 0x8248:	case 0x824a:	case 0x824c:	case 0x824e:
						case 0x8250:	case 0x8252:	case 0x8254:	case 0x8256:
						case 0x8258:	case 0x825a:	case 0x825c:	case 0x825e:
						case 0x8260:	case 0x8262:
							if(!cycle.data_select_active()) return delay;

							if(cycle.operation & Microcycle::Read) {
								cycle.set_value16(video_->read(int(address >> 1)));
							} else {
								video_->write(int(address >> 1), cycle.value16());
							}
						break;

						// DMA.
						case 0x8604:	case 0x8606:	case 0x8608:	case 0x860a:	case 0x860c:
							if(!cycle.data_select_active()) return delay;

							if(cycle.operation & Microcycle::Read) {
								cycle.set_value16(dma_->read(int(address >> 1)));
							} else {
								dma_->write(int(address >> 1), cycle.value16());
							}
						break;

						// Audio.
						//
						// Re: mirrors, Dan Hollis' hardware register list asserts:
						//
						// "Note: PSG Registers are now fixed at these addresses. All other addresses are masked out on the Falcon. Any
						// writes to the shadow registers $8804-$88FF will cause bus errors.", which I am taking to imply that those shadow
						// registers exist on the Atari ST.
						case 0x8800: case 0x8802: case 0x8804: case 0x8806: case 0x8808: case 0x880a: case 0x880c: case 0x880e:
						case 0x8810: case 0x8812: case 0x8814: case 0x8816: case 0x8818: case 0x881a: case 0x881c: case 0x881e:
						case 0x8820: case 0x8822: case 0x8824: case 0x8826: case 0x8828: case 0x882a: case 0x882c: case 0x882e:
						case 0x8830: case 0x8832: case 0x8834: case 0x8836: case 0x8838: case 0x883a: case 0x883c: case 0x883e:
						case 0x8840: case 0x8842: case 0x8844: case 0x8846: case 0x8848: case 0x884a: case 0x884c: case 0x884e:
						case 0x8850: case 0x8852: case 0x8854: case 0x8856: case 0x8858: case 0x885a: case 0x885c: case 0x885e:
						case 0x8860: case 0x8862: case 0x8864: case 0x8866: case 0x8868: case 0x886a: case 0x886c: case 0x886e:
						case 0x8870: case 0x8872: case 0x8874: case 0x8876: case 0x8878: case 0x887a: case 0x887c: case 0x887e:
						case 0x8880: case 0x8882: case 0x8884: case 0x8886: case 0x8888: case 0x888a: case 0x888c: case 0x888e:
						case 0x8890: case 0x8892: case 0x8894: case 0x8896: case 0x8898: case 0x889a: case 0x889c: case 0x889e:
						case 0x88a0: case 0x88a2: case 0x88a4: case 0x88a6: case 0x88a8: case 0x88aa: case 0x88ac: case 0x88ae:
						case 0x88b0: case 0x88b2: case 0x88b4: case 0x88b6: case 0x88b8: case 0x88ba: case 0x88bc: case 0x88be:
						case 0x88c0: case 0x88c2: case 0x88c4: case 0x88c6: case 0x88c8: case 0x88ca: case 0x88cc: case 0x88ce:
						case 0x88d0: case 0x88d2: case 0x88d4: case 0x88d6: case 0x88d8: case 0x88da: case 0x88dc: case 0x88de:
						case 0x88e0: case 0x88e2: case 0x88e4: case 0x88e6: case 0x88e8: case 0x88ea: case 0x88ec: case 0x88ee:
						case 0x88f0: case 0x88f2: case 0x88f4: case 0x88f6: case 0x88f8: case 0x88fa: case 0x88fc: case 0x88fe:

							if(!cycle.data_select_active()) return delay;

							advance_time(HalfCycles(2));
							update_audio();

							if(cycle.operation & Microcycle::Read) {
								cycle.set_value8_high(GI::AY38910::Utility::read(ay_));
							} else {
								// Net effect here: addresses with bit 1 set write to a register,
								// addresses with bit 1 clear select a register.
								GI::AY38910::Utility::write(ay_, address&2, cycle.value8_high());
							}
						return delay + HalfCycles(2);

						// The MFP block:
						case 0xfa00:	case 0xfa02:	case 0xfa04:	case 0xfa06:
						case 0xfa08:	case 0xfa0a:	case 0xfa0c:	case 0xfa0e:
						case 0xfa10:	case 0xfa12:	case 0xfa14:	case 0xfa16:
						case 0xfa18:	case 0xfa1a:	case 0xfa1c:	case 0xfa1e:
						case 0xfa20:	case 0xfa22:	case 0xfa24:	case 0xfa26:
						case 0xfa28:	case 0xfa2a:	case 0xfa2c:	case 0xfa2e:
						case 0xfa30:	case 0xfa32:	case 0xfa34:	case 0xfa36:
						case 0xfa38:	case 0xfa3a:	case 0xfa3c:	case 0xfa3e:
							if(!cycle.data_select_active()) return delay;

							if(cycle.operation & Microcycle::Read) {
								cycle.set_value8_low(mfp_->read(int(address >> 1)));
							} else {
								mfp_->write(int(address >> 1), cycle.value8_low());
							}
						break;

						// ACIAs.
						case 0xfc00:	case 0xfc02:	case 0xfc04:	case 0xfc06: {
							// Set VPA.
							mc68000_.set_is_peripheral_address(!cycle.data_select_active());
							if(!cycle.data_select_active()) return delay;

							const auto acia_ = (address & 4) ? &midi_acia_ : &keyboard_acia_;
							if(cycle.operation & Microcycle::Read) {
								cycle.set_value8_high((*acia_)->read(int(address >> 1)));
							} else {
								(*acia_)->write(int(address >> 1), cycle.value8_high());
							}
						} break;
					}
				return HalfCycles(0);
			}

			// If control has fallen through to here, the access is either a read from ROM, or a read or write to RAM.
			switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
				default:
				break;

				case Microcycle::SelectWord | Microcycle::Read:
					cycle.value->w = *reinterpret_cast<uint16_t *>(&memory[address]);
				break;
				case Microcycle::SelectByte | Microcycle::Read:
					cycle.value->b = memory[address];
				break;
				case Microcycle::SelectWord:
					if(address >= video_range_.low_address && address < video_range_.high_address)
						video_.flush();
					*reinterpret_cast<uint16_t *>(&memory[address]) = cycle.value->w;
				break;
				case Microcycle::SelectByte:
					if(address >= video_range_.low_address && address < video_range_.high_address)
						video_.flush();
					memory[address] = cycle.value->b;
				break;
			}

			return HalfCycles(0);
		}

		void flush_output(int outputs) final {
			dma_.flush();
			mfp_.flush();
			keyboard_acia_.flush();
			midi_acia_.flush();

			if(outputs & Output::Video) {
				video_.flush();
			}
			if(outputs & Output::Audio) {
				update_audio();
				audio_queue_.perform();
			}
		}

	private:
		forceinline void advance_time(HalfCycles length) {
			// Advance the relevant counters.
			cycles_since_audio_update_ += length;
			mfp_ += length;
			if(dma_clocking_preference_ != ClockingHint::Preference::None)
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

			if(dma_clocking_preference_ == ClockingHint::Preference::RealTime) {
				dma_.flush();
			}

			// Update the video output, checking whether a sequence point has been hit.
			if(video_.will_flush(length)) {
				length -= video_.cycles_until_implicit_flush();
				video_ += video_.cycles_until_implicit_flush();

				mfp_->set_timer_event_input(1, video_->display_enabled());
				update_interrupt_input();
			}

			video_ += length;
		}

		void update_audio() {
			speaker_.run_for(audio_queue_, cycles_since_audio_update_.divide_cycles(Cycles(4)));
		}

		CPU::MC68000Mk2::Processor<ConcreteMachine, true, true> mc68000_;
		HalfCycles bus_phase_;

		JustInTimeActor<Video> video_;

		// The MFP runs at 819200/2673749ths of the CPU clock rate.
		JustInTimeActor<Motorola::MFP68901::MFP68901, HalfCycles, 819200, 2673749> mfp_;
		JustInTimeActor<Motorola::ACIA::ACIA, HalfCycles, 16> keyboard_acia_;
		JustInTimeActor<Motorola::ACIA::ACIA, HalfCycles, 16> midi_acia_;

		Concurrency::AsyncTaskQueue<false> audio_queue_;
		GI::AY38910::AY38910<false> ay_;
		Outputs::Speaker::PullLowpass<GI::AY38910::AY38910<false>> speaker_;
		HalfCycles cycles_since_audio_update_;

		JustInTimeActor<DMAController> dma_;

		HalfCycles cycles_since_ikbd_update_;
		IntelligentKeyboard ikbd_;

		std::vector<uint8_t> ram_;
		std::vector<uint8_t> rom_;
		uint32_t rom_start_ = 0;

		enum class BusDevice {
			/// A mostly RAM page is one that returns ROM for the first 8 bytes, RAM elsewhere.
			MostlyRAM,
			/// Allows reads and writes to ram_.
			RAM,
			/// Nothing is mapped to this area, and it also doesn't trigger an exception upon access.
			Floating,
			/// Allows reading from rom_; writes do nothing.
			ROM,
			/// Allows interaction with a cartrige_.
			Cartridge,
			/// Marks the IO page, in which finer decoding will occur.
			IO,
			/// An unassigned page has nothing below it, in a way that triggers exceptions.
			Unassigned
		};
		BusDevice memory_map_[256];

		// MARK: - Clocking Management.
		bool may_defer_acias_ = true;
		bool keyboard_needs_clock_ = false;
		bool mfp_is_realtime_ = false;
		ClockingHint::Preference dma_clocking_preference_ = ClockingHint::Preference::None;
		void set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) final {
			// This is being called by one of the components; avoid any time flushing here as that's
			// already dealt with (and, just to be absolutely sure, to avoid recursive mania).
			may_defer_acias_ =
				(keyboard_acia_.last_valid()->preferred_clocking() != ClockingHint::Preference::RealTime) &&
				(midi_acia_.last_valid()->preferred_clocking() != ClockingHint::Preference::RealTime);
			keyboard_needs_clock_ = ikbd_.preferred_clocking() != ClockingHint::Preference::None;
			mfp_is_realtime_ = mfp_.last_valid()->preferred_clocking() == ClockingHint::Preference::RealTime;
			dma_clocking_preference_ = dma_.last_valid()->preferred_clocking();
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
				dma_->bus_grant(reinterpret_cast<uint16_t *>(ram_.data()), ram_.size() >> 1);
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
				0x80 |	// b7: Monochrome monitor detect (0 = is monochrome).
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
		void mfp68901_did_change_interrupt_status(Motorola::MFP68901::MFP68901 *) final {
			update_interrupt_input();
		}

		int video_interrupts_pending_ = 0;
		bool previous_hsync_ = false, previous_vsync_ = false;
		void update_interrupt_input() {
			// Complete guess: set video interrupts pending if/when hsync of vsync
			// go inactive. Reset upon IACK.
			const bool hsync = video_.last_valid()->hsync();
			const bool vsync = video_.last_valid()->vsync();
			if(previous_hsync_ != hsync && previous_hsync_) {
				video_interrupts_pending_ |= 2;
			}
			if(previous_vsync_ != vsync && previous_vsync_) {
				video_interrupts_pending_ |= 4;
			}
			previous_vsync_ = vsync;
			previous_hsync_ = hsync;

			if(mfp_->get_interrupt_line()) {
				mc68000_.set_interrupt_level(6);
			} else if(video_interrupts_pending_ & 4) {
				mc68000_.set_interrupt_level(4);
			} else if(video_interrupts_pending_ & 2) {
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

		// MARK: - JoystickMachine
		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final {
			return ikbd_.get_joysticks();
		}

		// MARK: - AYPortHandler
		void set_port_output(bool port_b, uint8_t value) final {
			if(port_b) {
				// TODO: ?
			} else {
				/*
					Port A:
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
		void set_activity_observer(Activity::Observer *observer) final {
			dma_->set_activity_observer(observer);
		}

		// MARK: - Video Range
		Video::Range video_range_;
		void video_did_change_access_range(Video *video) final {
			video_range_ = video->get_memory_access_range();
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			auto options = std::make_unique<Options>(Configurable::OptionsType::UserFriendly);
			options->output = get_video_signal_configurable();
			return options;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &str) final {
			const auto options = dynamic_cast<Options *>(str.get());
			set_video_signal_configurable(options->output);
		}
};

}
}

using namespace Atari::ST;

Machine *Machine::AtariST(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	auto *const atari_target = dynamic_cast<const Analyser::Static::AtariST::Target *>(target);
	if(!atari_target) {
		return nullptr;
	}

	return new ConcreteMachine(*atari_target, rom_fetcher);
}

Machine::~Machine() {}
