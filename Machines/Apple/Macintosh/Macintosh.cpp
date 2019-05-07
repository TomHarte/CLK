//
//  Macintosh.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Macintosh.hpp"

#include <array>

#include "Video.hpp"

#include "../../CRTMachine.hpp"

#include "../../../Processors/68000/68000.hpp"
#include "../../../Components/6522/6522.hpp"
#include "../../../Components/DiskII/IWM.hpp"

#include "../../Utility/MemoryPacker.hpp"

namespace Apple {
namespace Macintosh {

class ConcreteMachine:
	public Machine,
	public CRTMachine::Machine,
	public CPU::MC68000::BusHandler {
	public:
		ConcreteMachine(const ROMMachine::ROMFetcher &rom_fetcher) :
		 	mc68000_(*this),
		 	video_(ram_.data()),
		 	via_(via_port_handler_),
		 	via_port_handler_(*this),
		 	iwm_(7833600) {

			// Grab a copy of the ROM and convert it into big-endian data.
			const auto roms = rom_fetcher("Macintosh", { "mac128k.rom" });
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			roms[0]->resize(64*1024);
			Memory::PackBigEndian16(*roms[0], rom_.data());

			// The Mac runs at 7.8336mHz.
			set_clock_rate(7833600.0);
		}

		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			video_.set_scan_target(scan_target);
		}

		Outputs::Speaker::Speaker *get_speaker() override {
			return nullptr;
		}

		void run_for(const Cycles cycles) override {
			mc68000_.run_for(cycles);
		}

		using Microcycle = CPU::MC68000::Microcycle;

		HalfCycles perform_bus_operation(const Microcycle &cycle, int is_supervisor) {
			time_since_video_update_ += cycle.length;
			time_since_iwm_update_ += cycle.length;

			// The VIA runs at one-tenth of the 68000's clock speed, in sync with the E clock.
			// See: Guide to the Macintosh Hardware Family p149 (PDF p188).
			via_clock_ += cycle.length;
			via_.run_for(via_clock_.divide(HalfCycles(10)));

			// TODO: SCC is a divide-by-two.

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
									if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
								} else {
									via_.set_register(register_address, cycle.value->halves.low);
								}
							break;

							case 0x68f000:
								// The IWM; this is a purely polled device, so can be run on demand.
								iwm_.run_for(time_since_iwm_update_.flush_cycles());
								if(cycle.operation & Microcycle::Read) {
									cycle.value->halves.low = iwm_.read(register_address);
									if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
								} else {
									iwm_.write(register_address, cycle.value->halves.low);
								}
							break;

							default:
								if(cycle.operation & Microcycle::Read) {
									cycle.value->halves.low = 0xff;
									if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
								}
							break;
						}
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
							memory_base = ram_.data();
							word_address %= ram_.size();
						} else {
							memory_base = rom_.data();
							word_address %= rom_.size();

							// Disallow writes to ROM.
							if(!(operation & Microcycle::Read)) operation = 0;
						}

						switch(operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read | Microcycle::InterruptAcknowledge)) {
							default:
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
			video_.run_for(time_since_video_update_.flush());
		}

		void set_rom_is_overlay(bool rom_is_overlay) {
			ROM_is_overlay_ = rom_is_overlay;
		}

		void set_use_alternate_screen_buffer(bool use_alternate_screen_buffer) {
			video_.set_use_alternate_screen_buffer(use_alternate_screen_buffer);
		}

	private:
		class VIAPortHandler: public MOS::MOS6522::PortHandler {
			public:
				VIAPortHandler(ConcreteMachine &machine) : machine_(machine) {}

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
//							printf("6522 A: %02x\n", value);
							machine_.set_rom_is_overlay(!!(value & 0x10));
							machine_.set_use_alternate_screen_buffer(!(value & 0x40));
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
//							printf("6522 B: %02x\n", value);
						break;
					}
				}

				uint8_t get_port_input(Port port) {
					switch(port) {
						case Port::A:
//							printf("6522 r A\n");
						return 0xff;

						case Port::B:
//							printf("6522 r B\n");
						return 0x00;
					}
				}

				void set_control_line_output(Port port, Line line, bool value) {
//					printf("6522 line %c%d: %c\n", port ? 'B' : 'A', int(line), value ? 't' : 'f');
				}

			private:
				ConcreteMachine &machine_;
		};

		std::array<uint16_t, 32*1024> rom_;
		std::array<uint16_t, 64*1024> ram_;

		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;
		Video video_;

		MOS::MOS6522::MOS6522<VIAPortHandler> via_;
 		VIAPortHandler via_port_handler_;

		Apple::IWM iwm_;

 		HalfCycles via_clock_;
 		HalfCycles time_since_video_update_;
 		HalfCycles time_since_iwm_update_;

		bool ROM_is_overlay_ = true;
};

}
}

using namespace Apple::Macintosh;

Machine *Machine::Macintosh(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(rom_fetcher);
}

Machine::~Machine() {}
