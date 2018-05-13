//
//  CartridgeTigervision.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeTigervision_hpp
#define Atari2600_CartridgeTigervision_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class Tigervision: public BusExtender {
	public:
		Tigervision(uint8_t *rom_base, std::size_t rom_size) :
			BusExtender(rom_base, rom_size) {
			rom_ptr_[0] = rom_base + rom_size - 4096;
			rom_ptr_[1] = rom_ptr_[0] + 2048;
		}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if((address&0x1fff) == 0x3f) {
				int offset = ((*value) * 2048) & (rom_size_ - 1);
				rom_ptr_[0] = rom_base_ + offset;
				return;
			} else if((address&0x1000) && isReadOperation(operation)) {
				*value = rom_ptr_[(address >> 11)&1][address & 2047];
			}
		}

	private:
		uint8_t *rom_ptr_[2];
};

}
}

#endif /* Atari2600_CartridgeTigervision_hpp */
