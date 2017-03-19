//
//  CartridgeUnpaged.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeUnpaged_hpp
#define Atari2600_CartridgeUnpaged_hpp

#include "Cartridge.hpp"

namespace Atari2600 {

class CartridgeUnpaged: public Cartridge<CartridgeUnpaged> {
	public:
		CartridgeUnpaged(const std::vector<uint8_t> &rom) :
			Cartridge(rom) {}

		void perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value) {
			if(isReadOperation(operation) && (address & 0x1000)) {
				*value = rom_[address & (rom_.size() - 1)];
			}
		}
};

}

#endif /* Atari2600_CartridgeUnpaged_hpp */
