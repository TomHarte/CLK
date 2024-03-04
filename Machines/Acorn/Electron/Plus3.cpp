//
//  Plus3.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Plus3.hpp"

using namespace Electron;

Plus3::Plus3() : WD1770(P1770) {
	emplace_drives(2, 8000000, 300, 2);
	set_control_register(last_control_, 0xff);
}

void Plus3::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
	get_drive(drive).set_disk(disk);
}

void Plus3::set_control_register(uint8_t control) {
	//	bit 0 => enable or disable drive 1
	//	bit 1 => enable or disable drive 2
	//	bit 2 => side select
	//	bit 3 => single density select

	uint8_t changes = control ^ last_control_;
	last_control_ = control;
	set_control_register(control, changes);
}

void Plus3::set_control_register(uint8_t control, uint8_t changes) {
	if(changes&3) {
		set_drive(control&3);
	}

	// Select the side on both drives at once.
	if(changes & 0x04) {
		get_drive(0).set_head((control & 0x04) ? 1 : 0);
		get_drive(1).set_head((control & 0x04) ? 1 : 0);
	}

	if(changes & 0x08) set_is_double_density(!(control & 0x08));
}

void Plus3::set_motor_on(bool on) {
	// TODO: this status should transfer if the selected drive changes. But the same goes for
	// writing state, so plenty of work to do in general here.
	get_drive().set_motor_on(on);
}

void Plus3::set_activity_observer(Activity::Observer *observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}
