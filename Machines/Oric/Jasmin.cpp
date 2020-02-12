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
Jasmin::Jasmin() : DiskController(P1770, 8000000, Storage::Disk::Drive::ReadyType::ShugartRDY) {
	set_is_double_density(true);
	select_paged_item();
}

void Jasmin::write(int address, uint8_t value) {
	switch(address) {
		// Set side.
		case 0x3f8: {
			const int head = value & 1;
			for_all_drives([head] (Storage::Disk::Drive &drive, size_t) {
				drive.set_head(head);
			});
		} break;

		case 0x3f9:
			/* TODO: reset. */
		break;

		case 0x3fa: {
			// If b0, enable overlay RAM.
			enable_overlay_ram_ = value & 1;
			select_paged_item();
		} break;

		case 0x3fb:
			// If b0, disable BASIC ROM.
			disable_basic_rom_ = value & 1;
			select_paged_item();
		break;

		case 0x3fc: case 0x3fd: case 0x3fe: case 0x3ff:
			get_drive().set_motor_on(false);
			set_drive(1 << (address - 0x3fc));
			get_drive().set_motor_on(motor_on_);
		break;

		default:
			return WD::WD1770::write(address, value);
	}
}

void Jasmin::set_motor_on(bool on) {
	motor_on_ = on;
	get_drive().set_motor_on(motor_on_);
	if(observer_) {
		observer_->set_led_status("Jasmin", on);
	}
}

void Jasmin::set_activity_observer(Activity::Observer *observer) {
	observer_ = observer;
	if(observer) {
		observer->register_led("Jasmin");
		observer_->set_led_status("Jasmin", motor_on_);
	}
}
