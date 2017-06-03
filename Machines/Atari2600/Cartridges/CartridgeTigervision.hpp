//
//  CartridgeTigervision.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeTigervision_hpp
#define Atari2600_CartridgeTigervision_hpp

#include "Cartridge.hpp"

namespace Atari2600 {

class CartridgeTigervision: public Cartridge<CartridgeTigervision> {
	public:
		CartridgeTigervision(const std::vector<uint8_t> &rom) :
			Cartridge(rom) {
			rom_ptr_[0] = rom_.data() + rom_.size() - 4096;
			rom_ptr_[1] = rom_ptr_[0] + 2048;
		}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if((address&0x1fff) == 0x3f) {
				int offset = ((*value) * 2048) & (rom_.size() - 1);
				rom_ptr_[0] = rom_.data() + offset;
				return;
			} else if((address&0x1000) && isReadOperation(operation)) {
				*value = rom_ptr_[(address >> 11)&1][address & 2047];
			}
		}

	private:
		uint8_t *rom_ptr_[2];
};

}

#endif /* Atari2600_CartridgeTigervision_hpp */
