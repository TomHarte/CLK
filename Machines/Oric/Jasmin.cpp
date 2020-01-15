//
//  Jasmin.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Jasmin.hpp"

using namespace Oric;

// NB: there's some controversy here on WD1770 versus WD1772, but between those two I think
// the only difference is stepping rates, and it says 1770 on the schematic I'm looking at.
Jasmin::Jasmin() : DiskController(P1770, 8000000) {
	set_is_double_density(true);
}

void Jasmin::write(int address, uint8_t value) {
	switch(address) {
		// Set side.
		case 0x3f8:
			for(auto &drive : drives_) {
				if(drive) drive->set_head(value & 1);
			}
		break;

		case 0x3f9:
			/* TODO: reset. */
		break;

		case 0x3fa: {
			// If b0, enable overlay RAM.
			set_paging_flags((get_paging_flags() & BASICDisable) | ((value & 1) ? OverlayRAMEnable : 0));
		} break;

		case 0x3fb:
			// If b0, disable BASIC ROM.
			set_paging_flags((get_paging_flags() & OverlayRAMEnable) | ((value & 1) ? BASICDisable : 0));
		break;

		case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff: {
			if(drives_[selected_drive_]) drives_[selected_drive_]->set_motor_on(false);
			select_drive(size_t(address - 0x3fc));
			if(drives_[selected_drive_]) drives_[selected_drive_]->set_motor_on(motor_on_);
		} break;

		default:
			return WD::WD1770::write(address, value);
	}
}

void Jasmin::set_motor_on(bool on) {
	motor_on_ = on;
	if(drives_[selected_drive_]) drives_[selected_drive_]->set_motor_on(motor_on_);
}
