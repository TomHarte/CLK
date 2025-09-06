//
//  ASCII16kb.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/MSX/MemorySlotHandler.hpp"

namespace MSX::Cartridge {

class ASCII16kbROMSlotHandler: public MemorySlotHandler {
public:
	ASCII16kbROMSlotHandler(MSX::MemorySlot &slot) : slot_(slot) {}

	void write(const uint16_t address, const uint8_t value, const bool pc_is_outside_bios) final {
		switch(address >> 11) {
			default:
				if(pc_is_outside_bios) confidence_counter_.add_miss();
			break;
			case 0xc:
				if(pc_is_outside_bios) {
					hit_or_equivocal(address == 0x6000);
				}
				slot_.map(value * 0x4000, 0x4000, 0x4000);
			break;
			case 0xe:
				if(pc_is_outside_bios) {
					hit_or_equivocal(address == 0x7000 || address == 0x77ff);
				}
				slot_.map(value * 0x4000, 0x8000, 0x4000);
			break;
		}
	}

	virtual std::string debug_type() final {
		return "A16";
	}

private:
	MSX::MemorySlot &slot_;
};

}
