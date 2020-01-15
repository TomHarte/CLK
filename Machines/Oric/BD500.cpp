//
//  BD500.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "BD500.hpp"

using namespace Oric;

BD500::BD500() : DiskController(P1793, 9000000) {
	set_paged_item(PagedItem::DiskROM);
	set_is_double_density(true);
}

void BD500::write(int address, uint8_t value) {
	switch(address) {
		case 0x0320: case 0x0321: case 0x0322: case 0x0323:
			WD::WD1770::write(address, value);
		break;
	}
}

uint8_t BD500::read(int address) {
	switch(address) {
		case 0x0320: case 0x0321: case 0x0322: case 0x0323:
		return WD::WD1770::read(address);
	}

	return 0xff;
}

void BD500::set_head_load_request(bool head_load) {
	// Turn all motors on or off, and load the head instantly.
	for(auto &drive : drives_) {
		if(drive) drive->set_motor_on(head_load);
	}
	set_head_loaded(head_load);
}
