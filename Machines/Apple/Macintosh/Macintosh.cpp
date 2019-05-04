//
//  Macintosh.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Macintosh.hpp"

#include "../../../Processors/68000/68000.hpp"
#include "../../../Components/6522/6522.hpp"
#include "../../Utility/MemoryPacker.hpp"

namespace Apple {
namespace Macintosh {

class ConcreteMachine:
	public Machine,
	public CPU::MC68000::BusHandler {
	public:
		ConcreteMachine(const ROMMachine::ROMFetcher &rom_fetcher) :
		 	mc68000_(*this) {

			// Grab a copy of the ROM and convert it into big-endian data.
			const auto roms = rom_fetcher("Macintosh", { "mac128k.rom" });
			if(!roms[0]) {
				throw ROMMachine::Error::MissingROMs;
			}
			roms[0]->resize(64*1024);
			Memory::PackBigEndian16(*roms[0], rom_);
		}

	private:
		CPU::MC68000::Processor<ConcreteMachine, true> mc68000_;
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
