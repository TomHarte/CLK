//
//  CartridgeMegaBoy.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Cartridge.hpp"

namespace Atari2600::Cartridge {

class MegaBoy: public BusExtender {
public:
	MegaBoy(const uint8_t *const rom_base, const std::size_t rom_size) :
		BusExtender(rom_base, rom_size), rom_ptr_(rom_base), current_page_(0) {}

	void perform_bus_operation(
		const CPU::MOS6502::BusOperation operation,
		uint16_t address,
		uint8_t *const value
	) {
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
	const uint8_t *rom_ptr_;
	uint8_t current_page_;
};

}
