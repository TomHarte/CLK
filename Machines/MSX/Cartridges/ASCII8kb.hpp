//
//  ASCII8kb.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/MSX/MemorySlotHandler.hpp"

namespace MSX::Cartridge {

class ASCII8kbROMSlotHandler: public MemorySlotHandler {
public:
	ASCII8kbROMSlotHandler(MSX::MemorySlot &slot) : slot_(slot) {}

	void write(const uint16_t address, const uint8_t value, const bool pc_is_outside_bios) final {
		switch(address >> 11) {
			default:
				if(pc_is_outside_bios) confidence_counter_.add_miss();
			break;
			case 0xc:
				if(pc_is_outside_bios) {
					hit_or_equivocal(address == 0x6000 || address == 0x60ff);
				}
				slot_.map(value * 0x2000, 0x4000, 0x2000);
			break;
			case 0xd:
				if(pc_is_outside_bios) {
					hit_or_equivocal(address == 0x6800 || address == 0x68ff);
				}
				slot_.map(value * 0x2000, 0x6000, 0x2000);
			break;
			case 0xe:
				if(pc_is_outside_bios) {
					hit_or_equivocal(address == 0x7000 || address == 0x70ff);
				}
				slot_.map(value * 0x2000, 0x8000, 0x2000);
			break;
			case 0xf:
				if(pc_is_outside_bios) {
					hit_or_equivocal(address == 0x7800 || address == 0x78ff);
				}
				slot_.map(value * 0x2000, 0xa000, 0x2000);
			break;
		}
	}

	virtual std::string debug_type() final {
		return "A8";
	}

private:
	MSX::MemorySlot &slot_;
};

}
