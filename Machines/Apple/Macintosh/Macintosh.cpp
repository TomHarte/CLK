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
#include "SonyDrive.hpp"
#include "Video.hpp"

#include "../../CRTMachine.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../MediaTarget.hpp"
#include "../../MouseMachine.hpp"

#include "../../../Inputs/QuadratureMouse/QuadratureMouse.hpp"

//#define LOG_TRACE

#include "../../../Components/6522/6522.hpp"
#include "../../../Components/8530/z8530.hpp"
#include "../../../Components/DiskII/IWM.hpp"
#include "../../../Processors/68000/68000.hpp"

#include "../../../Analyser/Static/Macintosh/Target.hpp"

#include "../../Utility/MemoryPacker.hpp"
#include "../../Utility/MemoryFuzzer.hpp"

namespace {

const int CLOCK_RATE = 7833600;

}

namespace Apple {
namespace Macintosh {

template <Analyser::Static::Macintosh::Target::Model model> class ConcreteMachine:
	public Machine,
	public CRTMachine::Machine,
	public MediaTarget::Machine,
	public MouseMachine::Machine,
	public CPU::MC68000::BusHandler,
	public KeyboardMachine::MappedMachine {
	public:
		using Target = Analyser::Static::Macintosh::Target;

		ConcreteMachine(const Target &target, const ROMMachine::ROMFetcher &rom_fetcher) :
		 	mc68000_(*this),
		 	iwm_(CLOCK_RATE),
		 	video_(ram_, audio_, drive_speed_accumulator_),
		 	via_(via_port_handler_),
		 	via_port_handler_(*this, clock_, keyboard_, video_, audio_, iwm_, mouse_),
		 	drives_{
		 		{CLOCK_RATE, model >= Analyser::Static::Macintosh::Target::Model::Mac512ke},
		 		{CLOCK_RATE, model >= Analyser::Static::Macintosh::Target::Model::Mac512ke}
			},
			mouse_(1) {

			// Select a ROM name and determine the proper ROM and RAM sizes
			// based on the machine model.
			using Model = Analyser::Static::Macintosh::Target::Model;
			std::string rom_name;
			uint32_t ram_size, rom_size;
			switch(model) {
				default:
				case Model::Mac128k:
					ram_size = 128*1024;
					rom_size = 64*1024;
					rom_name = "mac128k.rom";
				break;
				case Model::Mac512k:
					ram_size = 512*1024;
					rom_size = 64*1024;
					rom_name = "mac512k.rom";
				break;
				case Model::Mac512ke:
				case Model::MacPlus:
					ram_size = 512*1024;
					rom_size = 128*1024;
					rom_name = "macplus.rom";
				break;
			}
			ram_mask_ = (ram_size >> 1) - 1;
			rom_mask_ = (rom_size >> 1) - 1;
			video_.set_ram_mask(ram_mask_);

			// Grab a copy of the ROM and convert it into big-endian data.
			const auto roms = rom_fetcher("Macintosh", { rom_name });
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			roms[0]->resize(rom_size);
			Memory::PackBigEndian16(*roms[0], rom_);

			// Randomise memory contents.
			Memory::Fuzz(ram_, sizeof(ram_) / sizeof(*ram_));

			// Attach the drives to the IWM.
			iwm_.iwm.set_drive(0, &drives_[0]);
			iwm_.iwm.set_drive(1, &drives_[1]);

			// The Mac runs at 7.8336mHz.
			set_clock_rate(double(CLOCK_RATE));
			audio_.speaker.set_input_rate(float(CLOCK_RATE));

			// Insert any supplied media.
			insert_media(target.media);
		}

		~ConcreteMachine() {
			audio_.queue.flush();
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			video_.set_scan_target(scan_target);
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return &audio_.speaker;
		}

		void run_for(const Cycles cycles) override {
			mc68000_.run_for(cycles);
		}

		using Microcycle = CPU::MC68000::Microcycle;

		HalfCycles perform_bus_operation(const Microcycle &cycle, int is_supervisor) {
			time_since_video_update_ += cycle.length;
			iwm_.time_since_update += cycle.length;

			// The VIA runs at one-tenth of the 68000's clock speed, in sync with the E clock.
			// See: Guide to the Macintosh Hardware Family p149 (PDF p188). Some extra division
			// may occur here in order to provide VSYNC at a proper moment.
			// Possibly route vsync.
			if(time_until_video_event_ >= cycle.length) {
				via_clock_ += cycle.length;
				via_.run_for(via_clock_.divide(HalfCycles(10)));
				time_until_video_event_ -= cycle.length;
			} else {
				auto cycles_to_progress = cycle.length;
				while(time_until_video_event_ < cycles_to_progress) {
					cycles_to_progress -= time_until_video_event_;

					via_clock_ += time_until_video_event_;
					via_.run_for(via_clock_.divide(HalfCycles(10)));

					video_.run_for(time_until_video_event_);
					time_since_video_update_ -= time_until_video_event_;
					time_until_video_event_ = video_.get_next_sequence_point();

					via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::One, !video_.vsync());
				}

				via_clock_ += cycles_to_progress;
				via_.run_for(via_clock_.divide(HalfCycles(10)));
				time_until_video_event_ -= cycles_to_progress;
			}

			// The keyboard also has a clock, albeit a very slow one — 100,000 cycles/second.
			// Its clock and data lines are connected to the VIA.
			keyboard_clock_ += cycle.length;
			const auto keyboard_ticks = keyboard_clock_.divide(HalfCycles(CLOCK_RATE / 100000));
			if(keyboard_ticks > HalfCycles(0)) {
				keyboard_.run_for(keyboard_ticks);
				via_.set_control_line_input(MOS::MOS6522::Port::B, MOS::MOS6522::Line::Two, keyboard_.get_data());
				via_.set_control_line_input(MOS::MOS6522::Port::B, MOS::MOS6522::Line::One, keyboard_.get_clock());
			}

			// Feed mouse inputs within at most 1250 cycles of each other.
			time_since_mouse_update_ += cycle.length;
			const auto mouse_ticks = time_since_mouse_update_.divide(HalfCycles(2500));
			if(mouse_ticks > HalfCycles(0)) {
				mouse_.prepare_step();
				scc_.set_dcd(0, mouse_.get_channel(1) & 1);
				scc_.set_dcd(1, mouse_.get_channel(0) & 1);
			}

			// TODO: SCC should be clocked at a divide-by-two, if and when it actually has
			// anything connected.

			// Consider updating the real-time clock.
			real_time_clock_ += cycle.length;
			auto ticks = real_time_clock_.divide_cycles(Cycles(CLOCK_RATE)).as_int();
			while(ticks--) {
				clock_.update();
				// TODO: leave a delay between toggling the input rather than using this coupled hack.
				via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::Two, true);
				via_.set_control_line_input(MOS::MOS6522::Port::A, MOS::MOS6522::Line::Two, false);
			}

			// Update interrupt input. TODO: move this into a VIA/etc delegate callback?
			// Double TODO: does this really cascade like this?
			if(scc_.get_interrupt_line()) {
				mc68000_.set_interrupt_level(2);
			} else if(via_.get_interrupt_line()) {
				mc68000_.set_interrupt_level(1);
			} else {
				mc68000_.set_interrupt_level(0);
			}
//			mc68000_.set_interrupt_level(
//				(via_.get_interrupt_line() ? 1 : 0) |
//				(scc_.get_interrupt_line() ? 2 : 0)
//				/* TODO: to emulate a programmer's switch: have it set bit 2 when pressed. */
//			);

			// A null cycle leaves nothing else to do.
			if(cycle.operation) {
				auto word_address = cycle.word_address();

				// Everything above E0 0000 is signalled as being on the peripheral bus.
				mc68000_.set_is_peripheral_address(word_address >= 0x700000);

				if(word_address >= 0x400000) {
					if(cycle.data_select_active()) {
						const int register_address = word_address >> 8;

						switch(word_address & 0x78f000) {
							case 0x70f000:
								// VIA accesses are via address 0xefe1fe + register*512,
								// which at word precision is 0x77f0ff + register*256.
								if(cycle.operation & Microcycle::Read) {
									cycle.value->halves.low = via_.get_register(register_address);
								} else {
									via_.set_register(register_address, cycle.value->halves.low);
								}
							break;

							case 0x68f000:
								// The IWM; this is a purely polled device, so can be run on demand.
#ifndef NDEBUG
//								printf("[%06x]: ", mc68000_.get_state().program_counter);
#endif
								iwm_.flush();
								if(cycle.operation & Microcycle::Read) {
									cycle.value->halves.low = iwm_.iwm.read(register_address);
								} else {
									iwm_.iwm.write(register_address, cycle.value->halves.low);
								}
							break;

							case 0x780000:
								// Phase read.
								if(cycle.operation & Microcycle::Read) {
									cycle.value->halves.low = phase_ & 7;
								}
							break;

							case 0x480000: case 0x48f000:
							case 0x580000: case 0x58f000:
								// Any word access here adjusts phase.
								if(cycle.operation & Microcycle::SelectWord) {
									++phase_;
								} else {
									if(word_address < 0x500000) {
										// A0 = 1 => reset; A0 = 0 => read.
										if(*cycle.address & 1) {
											scc_.reset();
										} else {
											const auto read = scc_.read(int(word_address));
											if(cycle.operation & Microcycle::Read) {
												cycle.value->halves.low = read;
											}
										}
									} else {
										if(*cycle.address & 1) {
											if(cycle.operation & Microcycle::Read) {
												scc_.write(int(word_address), 0xff);
											} else {
												scc_.write(int(word_address), cycle.value->halves.low);
											}
										}
									}
								}
							break;

							default:
								if(cycle.operation & Microcycle::Read) {
									printf("Unrecognised read %06x\n", *cycle.address & 0xffffff);
									cycle.value->halves.low = 0x00;
								} else {
									printf("Unrecognised write %06x\n", *cycle.address & 0xffffff);
								}
							break;
						}
						if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
					}
				} else {
					if(cycle.data_select_active()) {
						uint16_t *memory_base = nullptr;
						auto operation = cycle.operation;

						// When ROM overlay is enabled, the ROM begins at both $000000 and $400000,
						// and RAM is available at $600000.
						//
						// Otherwise RAM is mapped at $000000 and ROM from $400000.
						if(
							(ROM_is_overlay_ && word_address >= 0x300000) ||
							(!ROM_is_overlay_ && word_address < 0x200000)
						) {
							memory_base = ram_;
							word_address &= ram_mask_;
							update_video();
						} else {
							memory_base = rom_;
							word_address &= rom_mask_;

							// Disallow writes to ROM; also it doesn't mirror above 0x60000, ever.
							if(!(operation & Microcycle::Read) || word_address >= 0x300000) operation = 0;
						}

						const auto masked_operation = operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read | Microcycle::InterruptAcknowledge);
						switch(masked_operation) {
							default:
							break;

							// Catches the deliberation set of operation to 0 above.
							case 0: break;

							case Microcycle::InterruptAcknowledge | Microcycle::SelectByte:
								// The Macintosh uses autovectored interrupts.
								mc68000_.set_is_peripheral_address(true);
							break;

							case Microcycle::SelectWord | Microcycle::Read:
								cycle.value->full = memory_base[word_address];
							break;
							case Microcycle::SelectByte | Microcycle::Read:
								cycle.value->halves.low = uint8_t(memory_base[word_address] >> cycle.byte_shift());
							break;
							case Microcycle::SelectWord:
								memory_base[word_address] = cycle.value->full;
							break;
							case Microcycle::SelectByte:
								memory_base[word_address] = uint16_t(
									(cycle.value->halves.low << cycle.byte_shift()) |
									(memory_base[word_address] & cycle.untouched_byte_mask())
								);
							break;
						}

//						if(!(operation & Microcycle::Read) && (word_address == (0x00000172 >> 1))) {
//							if(operation & Microcycle::SelectByte)
//								printf("MBState: %02x\n", cycle.value->halves.low);
//							else
//								printf("MBState: %04x\n", cycle.value->full);
//						}
//						if(
//							(
//									(word_address == (0x00000352 >> 1))
//								||	(word_address == (0x00000354 >> 1))
//								||	(word_address == (0x00005d16 >> 1))
//							)
//						) {
//							printf("%s %08x: %04x from around %08x\n", (operation & Microcycle::Read) ? "Read" : "Write", word_address << 1, memory_base[word_address], mc68000_.get_state().program_counter);
//						}
					} else {
						// TODO: add delay if this is a RAM access and video blocks it momentarily.
						// "Each [video] fetch took two cycles out of eight"
					}
				}
			}

			/*
				Normal memory map:

				000000: 	RAM
				400000: 	ROM
				9FFFF8+:	SCC read operations
				BFFFF8+:	SCC write operations
				DFE1FF+:	IWM
				EFE1FE+:	VIA
			*/

			return HalfCycles(0);
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

			// Experimental?
			iwm_.flush();
		}

		void set_rom_is_overlay(bool rom_is_overlay) {
			ROM_is_overlay_ = rom_is_overlay;
		}

		bool video_is_outputting() {
			return video_.is_outputting(time_since_video_update_);
		}

		void set_use_alternate_buffers(bool use_alternate_screen_buffer, bool use_alternate_audio_buffer) {
			video_.set_use_alternate_buffers(use_alternate_screen_buffer, use_alternate_audio_buffer);
		}

		bool insert_media(const Analyser::Static::Media &media) override {
			if(media.disks.empty())
				return false;

			// TODO: shouldn't allow disks to be replaced like this, as the Mac
			// uses software eject. Will need to expand messaging ability of
			// insert_media.
			drives_[0].set_disk(media.disks[0]);

			return true;
		}

		// MARK: Keyboard input.

		KeyboardMapper *get_keyboard_mapper() override {
			return &keyboard_mapper_;
		}

		void set_key_state(uint16_t key, bool is_pressed) override {
			keyboard_.enqueue_key_state(key, is_pressed);
		}

		// TODO: clear all keys.

	private:
		void update_video() {
			video_.run_for(time_since_video_update_.flush());
			time_until_video_event_ = video_.get_next_sequence_point();
		}

		Inputs::Mouse &get_mouse() override {
			return mouse_;
		}

		struct IWM {
			IWM(int clock_rate) : iwm(clock_rate) {}

			Apple::IWM iwm;
			HalfCycles time_since_update;

			void flush() {
				iwm.run_for(time_since_update.flush_cycles());
			}
		};

		class VIAPortHandler: public MOS::MOS6522::PortHandler {
			public:
				VIAPortHandler(ConcreteMachine &machine, RealTimeClock &clock, Keyboard &keyboard, Video &video, DeferredAudio &audio, IWM &iwm, Inputs::QuadratureMouse &mouse) :
					machine_(machine), clock_(clock), keyboard_(keyboard), video_(video), audio_(audio), iwm_(iwm), mouse_(mouse) {}

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
							iwm_.flush();
							iwm_.iwm.set_select(!!(value & 0x20));

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
					else printf("Unhandled control line output: %c %d\n", port ? 'B' : 'A', int(line));
				}

				void run_for(HalfCycles duration) {
					audio_.time_since_update += duration;
				}

				void flush() {
					audio_.flush();
				}

			private:
				ConcreteMachine &machine_;
				RealTimeClock &clock_;
				Keyboard &keyboard_;
				Video &video_;
				DeferredAudio &audio_;
				IWM &iwm_;
				Inputs::QuadratureMouse &mouse_;
		};

		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;

		DriveSpeedAccumulator drive_speed_accumulator_;
		IWM iwm_;

		DeferredAudio audio_;
		Video video_;

		RealTimeClock clock_;
		Keyboard keyboard_;

		MOS::MOS6522::MOS6522<VIAPortHandler> via_;
 		VIAPortHandler via_port_handler_;

 		Zilog::SCC::z8530 scc_;

 		HalfCycles via_clock_;
 		HalfCycles real_time_clock_;
 		HalfCycles keyboard_clock_;
 		HalfCycles time_since_video_update_;
 		HalfCycles time_until_video_event_;
 		HalfCycles time_since_iwm_update_;
 		HalfCycles time_since_mouse_update_;

		bool ROM_is_overlay_ = true;
		int phase_ = 1;

		SonyDrive drives_[2];
		Inputs::QuadratureMouse mouse_;

		Apple::Macintosh::KeyboardMapper keyboard_mapper_;

		uint32_t ram_mask_ = 0;
		uint32_t rom_mask_ = 0;
		uint16_t rom_[64*1024];
		uint16_t ram_[256*1024];
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
