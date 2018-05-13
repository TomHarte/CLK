//
//  CartridgeMegaBoy.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_CartridgeMegaBoy_hpp
#define Atari2600_CartridgeMegaBoy_hpp

#include "Cartridge.hpp"

namespace Atari2600 {
namespace Cartridge {

class MegaBoy: public BusExtender {
	public:
		MegaBoy(uint8_t *rom_base, std::size_t rom_size) :
			BusExtender(rom_base, rom_size),
			rom_ptr_(rom_base),
			current_page_(0) {
		}

		void perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			address &= 0x1fff;
			if(!(address & 0x1000)) return;

			if(address == 0x1ff0) {
				current_page_ = (current_page_ + 1) & 15;
				rom_ptr_ = rom_base_ + current_page_ * 4096;
			}

			if(isReadOperation(operation)) {
				*value = rom_ptr_[address & 4095];
			}
		}

	private:
		uint8_t *rom_ptr_;
		uint8_t current_page_;
};

}
}

#endif /* CartridgeMegaBoy_h */
