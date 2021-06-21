//
//  EXDos.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "EXDos.hpp"

using namespace Enterprise;

EXDos::EXDos() : WD1770(P1770) {
	emplace_drives(4, 8000000, 300, 2);
	set_control_register(0x00);
}

void EXDos::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
	get_drive(drive).set_disk(disk);
}

void EXDos::set_control_register(uint8_t control) {
	printf("Control: %02x\n", control);

	last_control_ = control;

	// Set side.
	const int head = (control >> 4) & 1;
	for(size_t c = 0; c < 4; c++) {
		get_drive(c).set_head(head);
	}

	// Select drive.
	// TODO: should potentially be drives, plural.
	set_drive(0);
	for(int c = 1; c < 4; c++) {
		if(control & (1 << c)) {
			set_drive(c);
		}
	}

	// TODO: seems like bit 6 might be connected to the drive's RDY line?

	// TODO: does part of this register select double/single density mode?
	// Probably either bit 5 or bit 7 of the control register?
	set_is_double_density(true);
}

uint8_t EXDos::get_control_register() {
	return last_control_ | (get_drive().get_is_ready() ? 0x40 : 0x00);
}

void EXDos::set_motor_on(bool on) {
	// TODO: this status should transfer if the selected drive changes. But the same goes for
	// writing state, so plenty of work to do in general here.
	get_drive().set_motor_on(on);
}
