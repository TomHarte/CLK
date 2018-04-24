//
//  DiskII.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "DiskII.hpp"

#include <cstdio>

using namespace Apple;

void DiskII::set_control(Control control, bool on) {
	printf("Set control %d %s\n", control, on ? "on" : "off");
}

void DiskII::set_mode(Mode mode) {
	printf("Set mode %d\n", mode);
	state_ = (state_ & ~0x08) | ((mode == Mode::Write) ? 0x8 : 0x0);
}

void DiskII::select_drive(int drive) {
	printf("Select drive %d\n", drive);
}

void DiskII::set_data_register(uint8_t value) {
	printf("Set data register (?)\n");
	state_ |= 0x4;
	data_register_ = value;
}

uint8_t DiskII::get_shift_register() {
	printf("Get shift register (?)\n");
	state_ &= ~0x4;
	return shift_register_;
}

void DiskII::run_for(const Cycles cycles) {
/*
... address the P6 ROM with an index byte built up as:
+-------+-------+-------+-------+-------+-------+-------+-------+
| STATE | STATE | STATE | PULSE |  Q7   |  Q6   |  SR   | STATE |
| bit 0 | bit 2 | bit 3 |       |       |       |  MSB  | bit 1 |
+-------+-------+-------+-------+-------+-------+-------+-------+
    7       6       5       4       3       2       1       0

...

The bytes in the P6 ROM has the high four bits reversed compared to the BAPD charts, so you will have to reverse them after fetching the byte.

*/

	uint8_t command = 0;
	switch(command) {
		case 0x0:	shift_register_ = 0;	break;	// clear
		case 0x9:	shift_register_ = static_cast<uint8_t>(shift_register_ << 1);			break;	// shift left, bringing in a zero
		case 0xd:	shift_register_ = static_cast<uint8_t>((shift_register_ << 1) | 1);		break;	// shift left, bringing in a one
		case 0xa:
			shift_register_ = (shift_register_ >> 1) | (is_write_protected() ? 0x80 : 0x00);
		break;	// shift right, bringing in write protected status
		case 0xb:	shift_register_ = data_register_;				break;	// load
		default: break;
	}
}

bool DiskII::is_write_protected() {
	return true;
}
