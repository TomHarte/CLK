//
//  CartridgeCommaVid.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Cartridge.hpp"

namespace Atari2600::Cartridge {

class CommaVid: public BusExtender {
public:
	CommaVid(const uint8_t *const rom_base, const std::size_t rom_size) : BusExtender(rom_base, rom_size) {}

	void perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		uint16_t address,
		uint8_t *const value
	) {
		if(!(address & 0x1000)) return;
		address &= 0x1fff;

		if(address < 0x1400) {
			if(isReadOperation(operation)) *value = ram_[address & 1023];
			return;
		}

		if(address < 0x1800) {
			ram_[address & 1023] = *value;
			return;
		}

		if(isReadOperation(operation)) *value = rom_base_[address & 2047];
	}

private:
	uint8_t ram_[1024];
};

}
