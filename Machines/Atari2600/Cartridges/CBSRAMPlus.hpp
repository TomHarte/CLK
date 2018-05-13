//
//  CartridgeCBSRAMPlus.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeCBSRAMPlus_hpp
#define Atari2600_CartridgeCBSRAMPlus_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class CBSRAMPlus: public BusExtender {
	public:
		CBSRAMPlus(uint8_t *rom_base, std::size_t rom_size) : BusExtender(rom_base, rom_size), rom_ptr_(rom_base) {}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address >= 0x1ff8 && address <= 0x1ffa) rom_ptr_ = rom_base_ + (address - 0x1ff8) * 4096;

			if(isReadOperation(operation)) {
				*value = rom_ptr_[address & 4095];
			}

			if(address < 0x1100) ram_[address & 0xff] = *value;
			else if(address < 0x1200 && isReadOperation(operation)) *value = ram_[address & 0xff];
		}

	private:
		uint8_t *rom_ptr_;
		uint8_t ram_[256];
};

}
}

#endif /* Atari2600_CartridgeCBSRAMPlus_hpp */
