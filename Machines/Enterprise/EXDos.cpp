//
//  EXDos.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "EXDos.hpp"

// TODO: disk_did_change_ should be on the drive. Some drives report it.

using namespace Enterprise;

EXDos::EXDos() : WD1770(P1770) {
	emplace_drives(4, 8000000, 300, 2);
	set_control_register(0x00);
}

void EXDos::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
	get_drive(drive).set_disk(disk);
	disk_did_change_ = true;
}

// Documentation for the control register:
//
//	Write:
//		b7	in use				(???)
//		b6	disk change reset
//		b5	0 = double density, 1 = single density
//		b4	side 1 select
//		b3, b3, b1, b0	select drive 3, 2, 1, 0
//
//	Read:
//		b7 data request from WD1770
//		b6 disk change
//		b5, b4, b3, b2: not used
//		b1 interrupt request from WD1770
//		b0 drive ready

void EXDos::set_control_register(uint8_t control) {
	if(control & 0x40) disk_did_change_ = false;
	set_is_double_density(!(control & 0x20));

	// Set side.
	const int head = (control >> 4) & 1;
	for(size_t c = 0; c < 4; c++) {
		get_drive(c).set_head(head);
	}

	// Select drive, ensuring handover of the motor-on state.
	const bool motor_state = get_drive().get_motor_on();
	for_all_drives([] (Storage::Disk::Drive &drive, size_t) {
		drive.set_motor_on(false);
	});
	set_drive(control & 0xf);
	get_drive().set_motor_on(motor_state);
}

uint8_t EXDos::get_control_register() {
	const uint8_t status =
		(get_data_request_line() ? 0x80 : 0x00) |
		(disk_did_change_ ? 0x40 : 0x00) |
		(get_interrupt_request_line() ? 0x02 : 0x00) |
		(get_drive().get_is_ready() ? 0x01 : 0x00);

	return status;
}

void EXDos::set_motor_on(bool on) {
	get_drive().set_motor_on(on);
}

void EXDos::set_activity_observer(Activity::Observer *observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}
