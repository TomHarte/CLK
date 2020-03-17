//
//  Macintosh.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Macintosh.hpp"

#include <array>

#include "DeferredAudio.hpp"
#include "DriveSpeedAccumulator.hpp"
#include "Keyboard.hpp"
#include "RealTimeClock.hpp"
#include "Video.hpp"

#include "../../../Activity/Source.hpp"
#include "../../CRTMachine.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../MediaTarget.hpp"
#include "../../MouseMachine.hpp"
#include "../../../Configurable/Configurable.hpp"

#include "../../../Inputs/QuadratureMouse/QuadratureMouse.hpp"
#include "../../../Outputs/Log.hpp"

#include "../../../ClockReceiver/JustInTime.hpp"
#include "../../../ClockReceiver/ClockingHintSource.hpp"
#include "../../../Configurable/StandardOptions.hpp"

//#define LOG_TRACE

#include "../../../Components/5380/ncr5380.hpp"
#include "../../../Components/6522/6522.hpp"
#include "../../../Components/8530/z8530.hpp"
#include "../../../Components/DiskII/IWM.hpp"
#include "../../../Components/DiskII/MacintoshDoubleDensityDrive.hpp"
#include "../../../Processors/68000/68000.hpp"

#include "../../../Storage/MassStorage/SCSI/SCSI.hpp"
#include "../../../Storage/MassStorage/SCSI/DirectAccessDevice.hpp"
#include "../../../Storage/MassStorage/Encodings/MacintoshVolume.hpp"

#include "../../../Analyser/Static/Macintosh/Target.hpp"

#include "../../Utility/MemoryPacker.hpp"
#include "../../Utility/MemoryFuzzer.hpp"

namespace {

constexpr int CLOCK_RATE = 7833600;

}

namespace Apple {
namespace Macintosh {

//std::vector<std::unique_ptr<Configurable::Option>> get_options() {
//	return Configurable::standard_options(
//		static_cast<Configurable::StandardOptions>(Configurable::QuickBoot)
//	);
//}

std::unique_ptr<Reflection::Struct> get_options() {
	return nullptr;
}

template <Analyser::Static::Macintosh::Target::Model model> class ConcreteMachine:
	public Machine,
	public CRTMachine::Machine,
	public MediaTarget::Machine,
	public MouseMachine::Machine,
	public CPU::MC68000::BusHandler,
	public KeyboardMachine::MappedMachine,
	public Zilog::SCC::z8530::Delegate,
	public Activity::Source,
	public Configurable::Device,
	public DriveSpeedAccumulator::Delegate,
	public ClockingHint::Observer {
	public:
		using Target = Analyser::Static::Macintosh::Target;

		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
			KeyboardMachine::MappedMachine({
				Inputs::Keyboard::Key::LeftShift, Inputs::Keyboard::Key::RightShift,
				Inputs::Keyboard::Key::LeftOption, Inputs::Keyboard::Key::RightOption,
				Inputs::Keyboard::Key::LeftMeta, Inputs::Keyboard::Key::RightMeta,
			}),
		 	mc68000_(*this),
		 	iwm_(CLOCK_RATE),
		 	video_(audio_, drive_speed_accumulator_),
		 	via_(via_port_handler_),
		 	via_port_handler_(*this, clock_, keyboard_, audio_, iwm_, mouse_),
		 	scsi_bus_(CLOCK_RATE * 2),
		 	scsi_(scsi_bus_, CLOCK_RATE * 2),
		 	hard_drive_(scsi_bus_, 6 /* SCSI ID */),
		 	drives_{
		 		{CLOCK_RATE, model >= Analyser::Static::Macintosh::Target::Model::Mac512ke},
		 		{CLOCK_RATE, model >= Analyser::Static::Macintosh::Target::Model::Mac512ke}
			},
			mouse_(1) {

			// Select a ROM name and determine the proper ROM and RAM sizes
			// based on the machine model.
			using Model = Analyser::Static::Macintosh::Target::Model;
			const std::string machine_name = "Macintosh";
			uint32_t ram_size, rom_size;
			std::vector<ROMMachine::ROM> rom_descriptions;
			switch(model) {
				default:
				case Model::Mac128k:
					ram_size = 128*1024;
					rom_size = 64*1024;
					rom_descriptions.emplace_back(machine_name, "the Macintosh 128k ROM", "mac128k.rom", 64*1024, 0x6d0c8a28);
				break;
				case Model::Mac512k:
					ram_size = 512*1024;
					rom_size = 64*1024;
					rom_descriptions.emplace_back(machine_name, "the Macintosh 512k ROM", "mac512k.rom", 64*1024, 0xcf759e0d);
				break;
				case Model::Mac512ke:
				case Model::MacPlus: {
					ram_size = ((model == Model::MacPlus) ? 4096 : 512)*1024;
					rom_size = 128*1024;
					const std::initializer_list<uint32_t> crc32s = { 0x4fa5b399, 0x7cacd18f, 0xb2102e8e };
					rom_descriptions.emplace_back(machine_name, "the Macintosh Plus ROM", "macplus.rom", 128*1024, crc32s);
				} break;
			}
			ram_mask_ = ram_size - 1;
			rom_mask_ = rom_size - 1;
			ram_.resize(ram_size);
			video_.set_ram(reinterpret_cast<uint16_t *>(ram_.data()), ram_mask_ >> 1);

			// Grab a copy of the ROM and convert it into big-endian data.
			const auto roms = rom_fetcher(rom_descriptions);
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			roms[0]->resize(rom_size);
			Memory::PackBigEndian16(*roms[0], rom_);

			// Randomise memory contents.
			Memory::Fuzz(ram_);

			// Attach the drives to the IWM.
			iwm_->set_drive(0, &drives_[0]);
			iwm_->set_drive(1, &drives_[1]);

			// If they are 400kb drives, also attach them to the drive-speed accumulator.
			if(!drives_[0].is_800k() || !drives_[1].is_800k()) {
				drive_speed_accumulator_.set_delegate(this);
			}

			// Make sure interrupt changes from the SCC are observed.
			scc_.set_delegate(this);

			// Also watch for changes in clocking requirement from the SCSI chip.
			if constexpr (model == Analyser::Static::Macintosh::Target::Model::MacPlus) {
				scsi_bus_.set_clocking_hint_observer(this);
			}

			// The Mac runs at 7.8336mHz.
			set_clock_rate(double(CLOCK_RATE));
			audio_.speaker.set_input_rate(float(CLOCK_RATE) / 2.0f);

			// Insert any supplied media.
			insert_media(target.media);

			// Set the immutables of the memory map.
			setup_memory_map();
		}

		~ConcreteMachine() {
			audio_.queue.flush();
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) final {
			video_.set_scan_target(scan_target);
		}

		Outputs::Display::ScanStatus get_scaled_scan_status() const final {
			return video_.get_scaled_scan_status();
		}

		Outputs::Speaker::Speaker *get_speaker() final {
			return &audio_.speaker;
		}

		void run_for(const Cycles cycles) final {
			mc68000_.run_for(cycles);
		}

		using Microcycle = CPU::MC68000::Microcycle;

		forceinline HalfCycles perform_bus_operation(const Microcycle &cycle, int is_supervisor) {
			// Advance time.
			advance_time(cycle.length);

			// A null cycle leaves nothing else to do.
			if(!(cycle.operation & (Microcycle::NewAddress | Microcycle::SameAddress))) return HalfCycles(0);

			// Grab the address.
			auto address = cycle.host_endian_byte_address();

			// Everything above E0 0000 is signalled as being on the peripheral bus.
			mc68000_.set_is_peripheral_address(address >= 0xe0'0000);

			// All code below deals only with reads and writes — cycles in which a
			// data select is active. So quit now if this is not the active part of
			// a read or write.
			//
			// The 68000 uses 6800-style autovectored interrupts, so the mere act of
			// having set VPA above deals with those given that the generated address
			// for interrupt acknowledge cycles always has all bits set except the
			// lowest explicit address lines.
			if(!cycle.data_select_active() || (cycle.operation & Microcycle::InterruptAcknowledge)) return HalfCycles(0);

			// Grab the word-precision address being accessed.
			uint8_t *memory_base = nullptr;
			HalfCycles delay;
			switch(memory_map_[address >> 17]) {
				default: assert(false);

				case BusDevice::Unassigned:
					fill_unmapped(cycle);
				return delay;

				case BusDevice::VIA: {
					if(*cycle.address & 1) {
						fill_unmapped(cycle);
					} else {
						const int register_address = address >> 9;

						// VIA accesses are via address 0xefe1fe + register*512,
						// which at word precision is 0x77f0ff + register*256.
						if(cycle.operation & Microcycle::Read) {
							cycle.value->halves.low = via_.read(register_address);
						} else {
							via_.write(register_address, cycle.value->halves.low);
						}

						if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
					}
				} return delay;

				case BusDevice::PhaseRead: {
					if(cycle.operation & Microcycle::Read) {
						cycle.value->halves.low = phase_ & 7;
					}

					if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
				} return delay;

				case BusDevice::IWM: {
					if(*cycle.address & 1) {
						const int register_address = address >> 9;

						// The IWM; this is a purely polled device, so can be run on demand.
						if(cycle.operation & Microcycle::Read) {
							cycle.value->halves.low = iwm_->read(register_address);
						} else {
							iwm_->write(register_address, cycle.value->halves.low);
						}

						if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
					} else {
						fill_unmapped(cycle);
					}
				} return delay;

				case BusDevice::SCSI: {
					const int register_address = address >> 4;
					const bool dma_acknowledge = address & 0x200;

					// Even accesses = read; odd = write.
					if(*cycle.address & 1) {
						// Odd access => this is a write. Data will be in the upper byte.
						if(cycle.operation & Microcycle::Read) {
							scsi_.write(register_address, 0xff, dma_acknowledge);
						} else {
							if(cycle.operation & Microcycle::SelectWord) {
								scsi_.write(register_address, cycle.value->halves.high, dma_acknowledge);
							} else {
								scsi_.write(register_address, cycle.value->halves.low, dma_acknowledge);
							}
						}
					} else {
						// Even access => this is a read.
						if(cycle.operation & Microcycle::Read) {
							const auto result = scsi_.read(register_address, dma_acknowledge);
							if(cycle.operation & Microcycle::SelectWord) {
								// Data is loaded on the top part of the bus only.
								cycle.value->full = uint16_t((result << 8) | 0xff);
							} else {
								cycle.value->halves.low = result;
							}
						}
					}
				} return delay;

				case BusDevice::SCCReadResetPhase: {
					// Any word access here adjusts phase.
					if(cycle.operation & Microcycle::SelectWord) {
						adjust_phase();
					} else {
						// A0 = 1 => reset; A0 = 0 => read.
						if(*cycle.address & 1) {
							scc_.reset();

							if(cycle.operation & Microcycle::Read) {
								cycle.value->halves.low = 0xff;
							}
						} else {
							const auto read = scc_.read(int(address >> 1));
							if(cycle.operation & Microcycle::Read) {
								cycle.value->halves.low = read;
							}
						}
					}
				} return delay;

				case BusDevice::SCCWrite: {
					// Any word access here adjusts phase.
					if(cycle.operation & Microcycle::SelectWord) {
						adjust_phase();
					} else {
						if(*cycle.address & 1) {
							if(cycle.operation & Microcycle::Read) {
								scc_.write(int(address >> 1), 0xff);
								cycle.value->halves.low = 0xff;
							} else {
								scc_.write(int(address >> 1), cycle.value->halves.low);
							}
						} else {
							fill_unmapped(cycle);
						}
					}
				} return delay;

				case BusDevice::RAM: {
					// This is coupled with the Macintosh implementation of video; the magic
					// constant should probably be factored into the Video class.
					// It embodies knowledge of the fact that video (and audio) will always
					// be fetched from the final $d900 bytes of memory.
					// (And that ram_mask_ = ram size - 1).
					if(address > ram_mask_ - 0xd900)
						update_video();

					memory_base = ram_.data();
					address &= ram_mask_;

					// Apply a delay due to video contention if applicable; scheme applied:
					// only every other access slot is available during the period of video
					// output. I believe this to be correct for the 128k, 512k and Plus.
					// More research to do on other models.
					if(video_is_outputting() && ram_subcycle_ < 8) {
						delay = HalfCycles(8 - ram_subcycle_);
						advance_time(delay);
					}
				} break;

				case BusDevice::ROM: {
					if(!(cycle.operation & Microcycle::Read)) return delay;
					memory_base = rom_;
					address &= rom_mask_;
				} break;
			}

			// If control has fallen through to here, the access is either a read from ROM, or a read or write to RAM.
			switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
				default:
				break;

				case Microcycle::SelectWord | Microcycle::Read:
					cycle.value->full = *reinterpret_cast<uint16_t *>(&memory_base[address]);
				break;
				case Microcycle::SelectByte | Microcycle::Read:
					cycle.value->halves.low = memory_base[address];
				break;
				case Microcycle::SelectWord:
					*reinterpret_cast<uint16_t *>(&memory_base[address]) = cycle.value->full;
				break;
				case Microcycle::SelectByte:
					memory_base[address] = cycle.value->halves.low;
				break;
			}

			return delay;
		}

		void flush() {
			// Flush the video before the audio queue; in a Mac the
			// video is responsible for providing part of the
			// audio signal, so the two aren't as distinct as in
			// most machines.
			update_video();

			// As above: flush audio after video.
			via_.flush();
			audio_.queue.perform();

			// This avoids deferring IWM costs indefinitely, until
			// they become artbitrarily large.
			iwm_.flush();
		}

		void set_rom_is_overlay(bool rom_is_overlay) {
			ROM_is_overlay_ = rom_is_overlay;

			using Model = Analyser::Static::Macintosh::Target::Model;
			switch(model) {
				case Model::Mac128k:
				case Model::Mac512k:
				case Model::Mac512ke:
					populate_memory_map(0, [rom_is_overlay] (std::function<void(int target, BusDevice device)> map_to) {
						// Addresses up to $80 0000 aren't affected by this bit.
						if(rom_is_overlay) {
							// Up to $60 0000 mirrors of the ROM alternate with unassigned areas every $10 0000 byes.
							for(int c = 0; c < 0x600000; c += 0x100000) {
								map_to(c + 0x100000, (c & 0x100000) ? BusDevice::Unassigned : BusDevice::ROM);
							}
							map_to(0x800000, BusDevice::RAM);
						} else {
							map_to(0x400000, BusDevice::RAM);
							map_to(0x500000, BusDevice::ROM);
							map_to(0x800000, BusDevice::Unassigned);
						}
					});
				break;

				case Model::MacPlus:
					populate_memory_map(0, [rom_is_overlay] (std::function<void(int target, BusDevice device)> map_to) {
						// Addresses up to $80 0000 aren't affected by this bit.
						if(rom_is_overlay) {
							for(int c = 0; c < 0x580000; c += 0x20000) {
								map_to(c + 0x20000, ((c & 0x100000) || (c & 0x20000)) ? BusDevice::Unassigned : BusDevice::ROM);
							}
							map_to(0x600000, BusDevice::SCSI);
							map_to(0x800000, BusDevice::RAM);
						} else {
							map_to(0x400000, BusDevice::RAM);
							for(int c = 0x400000; c < 0x580000; c += 0x20000) {
								map_to(c + 0x20000, ((c & 0x100000) || (c & 0x20000)) ? BusDevice::Unassigned : BusDevice::ROM);
							}
							map_to(0x600000, BusDevice::SCSI);
							map_to(0x800000, BusDevice::Unassigned);
						}
					});
				break;
			}
		}

		bool video_is_outputting() {
			return video_.is_outputting(time_since_video_update_);
		}

		void set_use_alternate_buffers(bool use_alternate_screen_buffer, bool use_alternate_audio_buffer) {
			update_video();
			video_.set_use_alternate_buffers(use_alternate_screen_buffer, use_alternate_audio_buffer);
		}

		bool insert_media(const Analyser::Static::Media &media) final {
			if(media.disks.empty() && media.mass_storage_devices.empty())
				return false;

			// TODO: shouldn't allow disks to be replaced like this, as the Mac
			// uses software eject. Will need to expand messaging ability of
			// insert_media.
			if(!media.disks.empty()) {
				if(drives_[0].has_disk())
					drives_[1].set_disk(media.disks[0]);
				else
					drives_[0].set_disk(media.disks[0]);
			}

			// TODO: allow this only at machine startup?
			if(!media.mass_storage_devices.empty()) {
				const auto volume = dynamic_cast<Storage::MassStorage::Encodings::Macintosh::Volume *>(media.mass_storage_devices.front().get());
				if(volume) {
					volume->set_drive_type(Storage::MassStorage::Encodings::Macintosh::DriveType::SCSI);
				}
				hard_drive_->set_storage(media.mass_storage_devices.front());
			}

			return true;
		}

		// MARK: Keyboard input.

		KeyboardMapper *get_keyboard_mapper() final {
			return &keyboard_mapper_;
		}

		void set_key_state(uint16_t key, bool is_pressed) final {
			keyboard_.enqueue_key_state(key, is_pressed);
		}

		// TODO: clear all keys.

		// MARK: Interrupt updates.

		void did_change_interrupt_status(Zilog::SCC::z8530 *sender, bool new_status) final {
			update_interrupt_input();
		}

		void update_interrupt_input() {
			// Update interrupt input.
			// TODO: does this really cascade like this?
			if(scc_.get_interrupt_line()) {
				mc68000_.set_interrupt_level(2);
			} else if(via_.get_interrupt_line()) {
				mc68000_.set_interrupt_level(1);
			} else {
				mc68000_.set_interrupt_level(0);
			}
		}

		// MARK: - Activity Source
		void set_activity_observer(Activity::Observer *observer) final {
			iwm_->set_activity_observer(observer);

			if constexpr (model == Analyser::Static::Macintosh::Target::Model::MacPlus) {
				scsi_bus_.set_activity_observer(observer);
			}
		}

		// MARK: - Configuration options.
		std::unique_ptr<Reflection::Struct> get_options() final {
			return nullptr;
		}

		void set_options(const std::unique_ptr<Reflection::Struct> &options) final {
		}
//		std::vector<std::unique_ptr<Configurable::Option>> get_options() final {
//			return Apple::Macintosh::get_options();
//		}
//
//		void set_selections(const Configurable::SelectionSet &selections_by_option) final {
//			bool quick_boot;
//			if(Configurable::get_quick_boot(selections_by_option, quick_boot)) {
//				if(quick_boot) {
//					// Cf. Big Mess o' Wires' disassembly of the Mac Plus ROM, and the
//					// test at $E00. TODO: adapt as(/if?) necessary for other Macs.
//					ram_[0x02ae] = 0x40;
//					ram_[0x02af] = 0x00;
//					ram_[0x02b0] = 0x00;
//					ram_[0x02b1] = 0x00;
//				}
//			}
//		}
//
//		Configurable::SelectionSet get_accurate_selections() final {
//			Configurable::SelectionSet selection_set;
//			Configurable::append_quick_boot_selection(selection_set, false);
//			return selection_set;
//		}
//
//		Configurable::SelectionSet get_user_friendly_selections() final {
//			Configurable::SelectionSet selection_set;
//			Configurable::append_quick_boot_selection(selection_set, true);
//			return selection_set;
//		}

	private:
		void set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) final {
			scsi_bus_is_clocked_ = scsi_bus_.preferred_clocking() != ClockingHint::Preference::None;
		}

		void drive_speed_accumulator_set_drive_speed(DriveSpeedAccumulator *, float speed) final {
			iwm_.flush();
			drives_[0].set_rotation_speed(speed);
			drives_[1].set_rotation_speed(speed);
		}

		forceinline void adjust_phase() {
			++phase_;
		}

		forceinline void fill_unmapped(const Microcycle &cycle) {
			if(!(cycle.operation & Microcycle::Read)) return;
			cycle.set_value16(0xffff);
		}

		/// Advances all non-CPU components by @c duration half cycles.
		forceinline void advance_time(HalfCycles duration) {
			time_since_video_update_ += duration;
			iwm_ += duration;
			ram_subcycle_ = (ram_subcycle_ + duration.as_integral()) & 15;

			// The VIA runs at one-tenth of the 68000's clock speed, in sync with the E clock.
			// See: Guide to the Macintosh Hardware Family p149 (PDF p188). Some extra division
			// may occur here in order to provide VSYNC at a proper moment.
			// Possibly route vsync.
			if(time_since_video_update_ < time_until_video_event_) {
				via_clock_ += duration;
				via_.run_for(via_clock_.divide(HalfCycles(10)));
			} else {
				auto via_time_base = time_since_video_update_ - duration;
				auto via_cycles_outstanding = duration;
				while(time_until_video_event_ < time_since_video_update_) {
					const auto via_cycles = time_until_video_event_ - via_time_base;
					via_time_base = HalfCycles(0);
					via_cycles_outstanding -= via_cycles;

					via_clock_ += via_cycles;
					via_.run_for(via_clock_.divide(HalfCycles(10)));

					video_.run_for(time_until_video_event_);
					time_since_video_update_ -= time_until_video_event_;
					time_until_video_event_ = video_.get_next_sequence_point();

					via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::One, !video_.vsync());
				}

				via_clock_ += via_cycles_outstanding;
				via_.run_for(via_clock_.divide(HalfCycles(10)));
			}

			// The keyboard also has a clock, albeit a very slow one — 100,000 cycles/second.
			// Its clock and data lines are connected to the VIA.
			keyboard_clock_ += duration;
			const auto keyboard_ticks = keyboard_clock_.divide(HalfCycles(CLOCK_RATE / 100000));
			if(keyboard_ticks > HalfCycles(0)) {
				keyboard_.run_for(keyboard_ticks);
				via_.set_control_line_input(MOS::MOS6522::Port::B, MOS::MOS6522::Line::Two, keyboard_.get_data());
				via_.set_control_line_input(MOS::MOS6522::Port::B, MOS::MOS6522::Line::One, keyboard_.get_clock());
			}

			// Feed mouse inputs within at most 1250 cycles of each other.
			if(mouse_.has_steps()) {
				time_since_mouse_update_ += duration;
				const auto mouse_ticks = time_since_mouse_update_.divide(HalfCycles(2500));
				if(mouse_ticks > HalfCycles(0)) {
					mouse_.prepare_step();
					scc_.set_dcd(0, mouse_.get_channel(1) & 1);
					scc_.set_dcd(1, mouse_.get_channel(0) & 1);
				}
			}

			// TODO: SCC should be clocked at a divide-by-two, if and when it actually has
			// anything connected.

			// Consider updating the real-time clock.
			real_time_clock_ += duration;
			auto ticks = real_time_clock_.divide_cycles(Cycles(CLOCK_RATE)).as_integral();
			while(ticks--) {
				clock_.update();
				// TODO: leave a delay between toggling the input rather than using this coupled hack.
				via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::Two, true);
				via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::Two, false);
			}

			// Update the SCSI if currently active.
			if constexpr (model == Analyser::Static::Macintosh::Target::Model::MacPlus) {
				if(scsi_bus_is_clocked_) scsi_bus_.run_for(duration);
			}
		}

		forceinline void update_video() {
			video_.run_for(time_since_video_update_.flush<HalfCycles>());
			time_until_video_event_ = video_.get_next_sequence_point();
		}

		Inputs::Mouse &get_mouse() final {
			return mouse_;
		}

		using IWMActor = JustInTimeActor<IWM, 1, 1, HalfCycles, Cycles>;

		class VIAPortHandler: public MOS::MOS6522::PortHandler {
			public:
				VIAPortHandler(ConcreteMachine &machine, RealTimeClock &clock, Keyboard &keyboard, DeferredAudio &audio, IWMActor &iwm, Inputs::QuadratureMouse &mouse) :
					machine_(machine), clock_(clock), keyboard_(keyboard), audio_(audio), iwm_(iwm), mouse_(mouse) {}

				using Port = MOS::MOS6522::Port;
				using Line = MOS::MOS6522::Line;

				void set_port_output(Port port, uint8_t value, uint8_t direction_mask) {
					/*
						Peripheral lines: keyboard data, interrupt configuration.
						(See p176 [/215])
					*/
					switch(port) {
						case Port::A:
							/*
								Port A:
									b7:	[input] SCC wait/request (/W/REQA and /W/REQB wired together for a logical OR)
									b6:	0 = alternate screen buffer, 1 = main screen buffer
									b5:	floppy disk SEL state control (upper/lower head "among other things")
									b4:	1 = use ROM overlay memory map, 0 = use ordinary memory map
									b3:	0 = use alternate sound buffer, 1 = use ordinary sound buffer
									b2–b0:	audio output volume
							*/
							iwm_->set_select(!!(value & 0x20));

							machine_.set_use_alternate_buffers(!(value & 0x40), !(value&0x08));
							machine_.set_rom_is_overlay(!!(value & 0x10));

							audio_.flush();
							audio_.audio.set_volume(value & 7);
						break;

						case Port::B:
							/*
								Port B:
									b7:	0 = sound enabled, 1 = sound disabled
									b6:	[input] 0 = video beam in visible portion of line, 1 = outside
									b5:	[input] mouse y2
									b4:	[input] mouse x2
									b3:	[input] 0 = mouse button down, 1 = up
									b2:	0 = real-time clock enabled, 1 = disabled
									b1:	clock's data-clock line
									b0:	clock's serial data line
							*/
							if(value & 0x4) clock_.abort();
							else clock_.set_input(!!(value & 0x2), !!(value & 0x1));

							audio_.flush();
							audio_.audio.set_enabled(!(value & 0x80));
						break;
					}
				}

				uint8_t get_port_input(Port port) {
					switch(port) {
						case Port::A:
//							printf("6522 r A\n");
						return 0x00;	// TODO: b7 = SCC wait/request

						case Port::B:
						return uint8_t(
							((mouse_.get_button_mask() & 1) ? 0x00 : 0x08) |
							((mouse_.get_channel(0) & 2) << 3) |
							((mouse_.get_channel(1) & 2) << 4) |
							(clock_.get_data() ? 0x02 : 0x00) |
							(machine_.video_is_outputting() ? 0x00 : 0x40)
						);
					}

					// Should be unreachable.
					return 0xff;
				}

				void set_control_line_output(Port port, Line line, bool value) {
					/*
						Keyboard wiring (I believe):
						CB2 = data		(input/output)
						CB1 = clock		(input)

						CA2 is used for receiving RTC interrupts.
						CA1 is used for receiving vsync.
					*/
					if(port == Port::B && line == Line::Two) {
						keyboard_.set_input(value);
					}
					else LOG("Unhandled control line output: " << (port ? 'B' : 'A') << int(line));
				}

				void run_for(HalfCycles duration) {
					// The 6522 enjoys a divide-by-ten, so multiply back up here to make the
					// divided-by-two clock the audio works on.
					audio_.time_since_update += HalfCycles(duration.as_integral() * 5);
				}

				void flush() {
					audio_.flush();
				}

				void set_interrupt_status(bool status) {
					machine_.update_interrupt_input();
				}

			private:
				ConcreteMachine &machine_;
				RealTimeClock &clock_;
				Keyboard &keyboard_;
				DeferredAudio &audio_;
				IWMActor &iwm_;
				Inputs::QuadratureMouse &mouse_;
		};

		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;

		DriveSpeedAccumulator drive_speed_accumulator_;
		IWMActor iwm_;

		DeferredAudio audio_;
		Video video_;

		RealTimeClock clock_;
		Keyboard keyboard_;

		MOS::MOS6522::MOS6522<VIAPortHandler> via_;
 		VIAPortHandler via_port_handler_;

 		Zilog::SCC::z8530 scc_;
		SCSI::Bus scsi_bus_;
 		NCR::NCR5380::NCR5380 scsi_;
		SCSI::Target::Target<SCSI::DirectAccessDevice> hard_drive_;
 		bool scsi_bus_is_clocked_ = false;

 		HalfCycles via_clock_;
 		HalfCycles real_time_clock_;
 		HalfCycles keyboard_clock_;
 		HalfCycles time_since_video_update_;
 		HalfCycles time_until_video_event_;
 		HalfCycles time_since_mouse_update_;

		bool ROM_is_overlay_ = true;
		int phase_ = 1;
		int ram_subcycle_ = 0;

		DoubleDensityDrive drives_[2];
		Inputs::QuadratureMouse mouse_;

		Apple::Macintosh::KeyboardMapper keyboard_mapper_;

		enum class BusDevice {
			RAM, ROM, VIA, IWM, SCCWrite, SCCReadResetPhase, SCSI, PhaseRead, Unassigned
		};

		/// Divides the 24-bit address space up into $20000 (i.e. 128kb) segments, recording
		/// which device is current mapped in each area. Keeping it in a table is a bit faster
		/// than the multi-level address inspection that is otherwise required, as well as
		/// simplifying slightly the handling of different models.
		///
		/// So: index with the top 7 bits of the 24-bit address.
		BusDevice memory_map_[128];

		void setup_memory_map() {
			// Apply the power-up memory map, i.e. assume that ROM_is_overlay_ = true;
			// start by calling into set_rom_is_overlay to seed everything up to $800000.
			set_rom_is_overlay(true);

			populate_memory_map(0x800000, [] (std::function<void(int target, BusDevice device)> map_to) {
				map_to(0x900000, BusDevice::Unassigned);
				map_to(0xa00000, BusDevice::SCCReadResetPhase);
				map_to(0xb00000, BusDevice::Unassigned);
				map_to(0xc00000, BusDevice::SCCWrite);
				map_to(0xd00000, BusDevice::Unassigned);
				map_to(0xe00000, BusDevice::IWM);
				map_to(0xe80000, BusDevice::Unassigned);
				map_to(0xf00000, BusDevice::VIA);
				map_to(0xf80000, BusDevice::PhaseRead);
				map_to(0x1000000, BusDevice::Unassigned);
			});
		}

		void populate_memory_map(int start_address, std::function<void(std::function<void(int, BusDevice)>)> populator) {
			// Define semantics for below; map_to will write from the current cursor position
			// to the supplied 24-bit address, setting a particular mapped device.
			int segment = start_address >> 17;
			auto map_to = [&segment, this](int address, BusDevice device) {
				for(; segment < address >> 17; ++segment) {
					this->memory_map_[segment] = device;
				}
			};

			populator(map_to);
		}

		uint32_t ram_mask_ = 0;
		uint32_t rom_mask_ = 0;
		uint8_t rom_[128*1024];
		std::vector<uint8_t> ram_;
};

}
}

using namespace Apple::Macintosh;

Machine *Machine::Macintosh(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	auto *const mac_target = dynamic_cast<const Analyser::Static::Macintosh::Target *>(target);

	using Model = Analyser::Static::Macintosh::Target::Model;
	switch(mac_target->model) {
		default:
		case Model::Mac128k:	return new ConcreteMachine<Model::Mac128k>(*mac_target, rom_fetcher);
		case Model::Mac512k:	return new ConcreteMachine<Model::Mac512k>(*mac_target, rom_fetcher);
		case Model::Mac512ke:	return new ConcreteMachine<Model::Mac512ke>(*mac_target, rom_fetcher);
		case Model::MacPlus:	return new ConcreteMachine<Model::MacPlus>(*mac_target, rom_fetcher);
	}
}

Machine::~Machine() {}
