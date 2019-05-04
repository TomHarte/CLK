//
//  Macintosh.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Macintosh.hpp"

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
		 	video_(ram_),
		 	via_(via_port_handler_) {

			// Grab a copy of the ROM and convert it into big-endian data.
			const auto roms = rom_fetcher("Macintosh", { "mac128k.rom" });
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			roms[0]->resize(64*1024);
			Memory::PackBigEndian16(*roms[0], rom_);

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

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			via_.run_for(cycle.length);

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
		class VIAPortHandler: public MOS::MOS6522::PortHandler {
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

		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;
		Video video_;

		MOS::MOS6522::MOS6522<VIAPortHandler> via_;
 		VIAPortHandler via_port_handler_;

		uint16_t rom_[32*1024];
		uint16_t ram_[64*1024];
};

}
}

using namespace Apple::Macintosh;

Machine *Machine::Macintosh(const Analyser::Static::Target *target, const ROMMachine::ROMFetcher &rom_fetcher) {
	return new ConcreteMachine(rom_fetcher);
}

Machine::~Machine() {}
