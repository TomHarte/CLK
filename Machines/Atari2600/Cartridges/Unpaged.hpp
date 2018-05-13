//
//  CartridgeUnpaged.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeUnpaged_hpp
#define Atari2600_CartridgeUnpaged_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class Unpaged: public BusExtender {
	public:
		Unpaged(uint8_t *rom_base, std::size_t rom_size) : BusExtender(rom_base, rom_size) {}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(isReadOperation(operation) && (address & 0x1000)) {
				*value = rom_base_[address & (rom_size_ - 1)];
			}
		}
};

}
}

#endif /* Atari2600_CartridgeUnpaged_hpp */
