//
//  CartridgeAtari8k.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Cartridge.hpp"

namespace Atari2600::Cartridge {

class Atari32k: public BusExtender {
public:
	Atari32k(const uint8_t *const rom_base, const std::size_t rom_size)
		: BusExtender(rom_base, rom_size), rom_ptr_(rom_base) {}

	void perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		uint16_t address,
		uint8_t *const value
	) {
		address &= 0x1fff;
		if(!(address & 0x1000)) return;

		if(address >= 0x1ff4 && address <= 0x1ffb) rom_ptr_ = rom_base_ + (address - 0x1ff4) * 4096;

		if(is_read(operation)) {
			*value = rom_ptr_[address & 4095];
		}
	}

private:
	const uint8_t *rom_ptr_;
};

class Atari32kSuperChip: public BusExtender {
public:
	Atari32kSuperChip(const uint8_t *const rom_base, const std::size_t rom_size)
		: BusExtender(rom_base, rom_size), rom_ptr_(rom_base) {}

	void perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		uint16_t address,
		uint8_t *const value
	) {
		address &= 0x1fff;
		if(!(address & 0x1000)) return;

		if(address >= 0x1ff4 && address <= 0x1ffb) rom_ptr_ = rom_base_ + (address - 0x1ff4) * 4096;

		if(is_read(operation)) {
			*value = rom_ptr_[address & 4095];
		}

		if(address < 0x1080) ram_[address & 0x7f] = *value;
		else if(address < 0x1100 && is_read(operation)) *value = ram_[address & 0x7f];
	}

private:
	const uint8_t *rom_ptr_;
	uint8_t ram_[128];
};

}
