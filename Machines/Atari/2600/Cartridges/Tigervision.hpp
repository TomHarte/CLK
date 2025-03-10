//
//  CartridgeTigervision.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Cartridge.hpp"

namespace Atari2600::Cartridge {

class Tigervision: public BusExtender {
public:
	Tigervision(const uint8_t *const rom_base, const std::size_t rom_size) : BusExtender(rom_base, rom_size) {
		rom_ptr_[0] = rom_base + rom_size - 4096;
		rom_ptr_[1] = rom_ptr_[0] + 2048;
	}

	void perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		const uint16_t address,
		uint8_t *const value
	) {
		if((address&0x1fff) == 0x3f) {
			int offset = ((*value) * 2048) & (rom_size_ - 1);
			rom_ptr_[0] = rom_base_ + offset;
			return;
		} else if((address&0x1000) && is_read(operation)) {
			*value = rom_ptr_[(address >> 11)&1][address & 2047];
		}
	}

private:
	const uint8_t *rom_ptr_[2];
};

}
