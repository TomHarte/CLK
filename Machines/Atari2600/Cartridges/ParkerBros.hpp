//
//  CartridgeParkerBros.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeParkerBros_hpp
#define Atari2600_CartridgeParkerBros_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class ParkerBros: public BusExtender {
	public:
		ParkerBros(uint8_t *rom_base, std::size_t rom_size) :
			BusExtender(rom_base, rom_size) {
			rom_ptr_[0] = rom_base + 4096;
			rom_ptr_[1] = rom_ptr_[0] + 1024;
			rom_ptr_[2] = rom_ptr_[1] + 1024;
			rom_ptr_[3] = rom_ptr_[2] + 1024;
		}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address >= 0x1fe0 && address < 0x1ff8) {
				int slot = (address >> 3)&3;
				rom_ptr_[slot] = rom_base_ + ((address & 7) * 1024);
			}

			if(isReadOperation(operation)) {
				*value = rom_ptr_[(address >> 10)&3][address & 1023];
			}
		}

	private:
		uint8_t *rom_ptr_[4];
};

}
}

#endif /* Atari2600_CartridgeParkerBros_hpp */
