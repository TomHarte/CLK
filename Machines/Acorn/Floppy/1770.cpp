//
//  Floppy1770.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "1770.hpp"

using namespace Acorn::Floppy;

Floppy1770::Floppy1770() : WD1770(P1770) {
	emplace_drives(2, 8000000, 300, 2);
	set_control_register(last_control_, 0xff);
}

void Floppy1770::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, const size_t drive) {
	get_drive(drive).set_disk(disk);
}

const Storage::Disk::Disk *Floppy1770::disk(const std::string &name) {
	const Storage::Disk::Disk *result = nullptr;
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		const auto disk = drive.disk();
		if(disk && disk->represents(name)) {
			result = disk;
		}
	});
	return result;
}


void Floppy1770::set_control_register(const uint8_t control) {
	//	bit 0 => enable or disable drive 1
	//	bit 1 => enable or disable drive 2
	//	bit 2 => side select
	//	bit 3 => single density select
	//	bit 5 => reset?

	uint8_t changes = control ^ last_control_;
	last_control_ = control;
	set_control_register(control, changes);
}

void Floppy1770::set_control_register(const uint8_t control, const uint8_t changes) {
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

void Floppy1770::set_motor_on(const bool on) {
	// TODO: this status should transfer if the selected drive changes. But the same goes for
	// writing state, so plenty of work to do in general here.
	get_drive().set_motor_on(on);
}

void Floppy1770::set_activity_observer(Activity::Observer *const observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}
