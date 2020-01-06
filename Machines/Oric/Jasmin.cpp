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
Jasmin::Jasmin() : WD1770(P1770) {
	set_is_double_density(true);
}

void Jasmin::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int d) {
	const size_t drive = size_t(d);
	if(!drives_[drive]) {
		drives_[drive] = std::make_unique<Storage::Disk::Drive>(8000000, 300, 2);
		if(drive == selected_drive_) set_drive(drives_[drive]);
	}
	drives_[drive]->set_disk(disk);
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
			posit_paging_flags((paging_flags_ & BASICDisable) | ((value & 1) ? OverlayRAMEnable : 0));
		} break;

		case 0x3fb:
			// If b0, disable BASIC ROM.
			posit_paging_flags((paging_flags_ & OverlayRAMEnable) | ((value & 1) ? BASICDisable : 0));
		break;

		case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff: {
			const size_t new_selected_drive = size_t(address - 0x3fc);

			if(new_selected_drive != selected_drive_) {
				if(drives_[selected_drive_]) drives_[selected_drive_]->set_motor_on(false);
				selected_drive_ = new_selected_drive;
				set_drive(drives_[selected_drive_]);

				// TODO: establish motor status for new drive.
			}
		} break;

		default:
			return WD::WD1770::write(address, value);
	}
}
