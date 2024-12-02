//
//  CartridgeUnpaged.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Cartridge.hpp"

namespace Atari2600::Cartridge {

class Unpaged: public BusExtender {
public:
	Unpaged(const uint8_t *const rom_base, const std::size_t rom_size) : BusExtender(rom_base, rom_size) {}

	void perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		const uint16_t address,
		uint8_t *const value
	) {
		if(isReadOperation(operation) && (address & 0x1000)) {
			*value = rom_base_[address & (rom_size_ - 1)];
		}
	}
};

}
