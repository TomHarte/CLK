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
		 	via_(via_port_handler_) {

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
			// Assumption here: it's a divide by ten to derive the 6522 clock, i.e.
			// it runs off the 68000's E clock.
			via_clock_ += cycle.length;
			via_.run_for(via_clock_.divide(HalfCycles(10)));

			// SCC is a divide-by-two.

			// A null cycle leaves nothing else to do.
			if(cycle.operation) {
				auto word_address = cycle.word_address();

				// Hardware devices begin at 0x800000.
				mc68000_.set_is_peripheral_address(word_address >= 0x400000);
				if(word_address >= 0x400000) {
					if(cycle.data_select_active()) {
						printf("IO access to %06x: ", word_address << 1);

						// VIA accesses are via address 0xefe1fe + register*512,
						// which at word precision is 0x77f0ff + register*256.
						if((word_address & 0x77f0ff) == 0x77f0ff) {
							printf("VIA access ");
							if(cycle.operation & Microcycle::Read) {
								cycle.value->halves.low = via_.get_register(word_address >> 8);
								if(cycle.operation & Microcycle::SelectWord) cycle.value->halves.high = 0xff;
							} else {
								via_.set_register(word_address >> 8, cycle.value->halves.low);
							}
						}

						printf("\n");
					}
				} else {
					if(cycle.data_select_active()) {
						uint16_t *memory_base = nullptr;
						bool is_read_only = false;

						// When ROM overlay is enabled, the ROM begins at both $000000 and $400000,
						// and RAM is available at $600000.
						//
						// Otherwise RAM is mapped at $000000 and ROM from $400000.
						if((ROM_is_overlay_ && word_address < 0x600000) || (!ROM_is_overlay_ && word_address & 0x200000)) {
							memory_base = rom_.data();
							word_address %= rom_.size();
							is_read_only = true;
						} else {
							memory_base = ram_.data();
							word_address %= ram_.size();
							is_read_only = false;
						}

						if(!is_read_only || (cycle.operation & Microcycle::Read)) {
							switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read | Microcycle::InterruptAcknowledge)) {
								default: break;

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
										(memory_base[word_address] & (0xffff ^ cycle.byte_mask()))
									);
								break;
							}
						}
					} else {
						// Add delay if this is a RAM access and video blocks it momentarily.
					}
				}
			}

			// Any access to the

			// TODO: the entirety of dealing with this cycle.

			/*
				Normal memory map:

				000000: 	RAM
				400000: 	ROM
				9FFFF8+:	SCC read operations
				BFFFF8+:	SCC write operations
				DFE1FF+:	IWM
				EFE1FE+:	VIA

				Overlay mode:

				ROM replaces RAM at 00000, while also being at 400000
			*/

			return HalfCycles(0);
		}

		/*
			Notes to self: accesses to the VIA are via the 68000's
			synchronous bus.
		*/

	private:
		struct VIAPortHandler: public MOS::MOS6522::PortHandler {
			void set_port_output(MOS::MOS6522::Port port, uint8_t value, uint8_t direction_mask) {
				/*
					Port A:
						b7:	[input] SCC wait/request (/W/REQA and /W/REQB wired together for a logical OR)
						b6:	0 = alternate screen buffer, 1 = main screen buffer
						b5:	floppy disk SEL state control (upper/lower head "among other things")
						b4:	1 = use ROM overlay memory map, 0 = use ordinary memory map
						b3:	0 = use alternate sound buffer, 1 = use ordinary sound buffer
						b2–b0:	audio output volume

					Port B:
						b7:	0 = sound enabled, 1 = sound disabled
						b6:	[input] 0 = video beam in visible portion of line, 1 = outside
						b5:	[input] mouse y2
						b4:	[input] mouse x2
						b3:	[input] 0 = mouse button down, 1 = up
						b2:	0 = real-time clock enabled, 1 = disabled
						b1:	clock's data-clock line
						b0:	clock's serial data line

					Peripheral lines: keyboard data, interrupt configuration.
					(See p176 [/215])
				*/
			}

		};

		std::array<uint16_t, 32*1024> rom_;
		std::array<uint16_t, 64*1024> ram_;

		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;
		Video video_;

		MOS::MOS6522::MOS6522<VIAPortHandler> via_;
 		VIAPortHandler via_port_handler_;
 		HalfCycles via_clock_;

		bool ROM_is_overlay_ = true;
};

}
}

using namespace Apple::Macintosh;

Machine *Machine::Macintosh(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(rom_fetcher);
}

Machine::~Machine() {}
