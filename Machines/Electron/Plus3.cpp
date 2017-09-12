//
//  Plus3.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Plus3.hpp"

using namespace Electron;

Plus3::Plus3() : WD1770(P1770) {
	set_control_register(last_control_, 0xff);
}

void Plus3::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive) {
	if(!drives_[drive]) {
		drives_[drive].reset(new Storage::Disk::Drive(8000000, 300));
		if(drive == selected_drive_) set_drive(drives_[drive]);
	}
	drives_[drive]->set_disk(disk);
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
		switch(control&3) {
			case 0:		selected_drive_ = -1;	set_drive(nullptr);		break;
			default:	selected_drive_ = 0;	set_drive(drives_[0]);	break;
			case 2:		selected_drive_ = 1;	set_drive(drives_[1]);	break;
		}
	}
	if(changes & 0x04) {
		if(drives_[0]) drives_[0]->set_head((control & 0x04) ? 1 : 0);
		if(drives_[1]) drives_[1]->set_head((control & 0x04) ? 1 : 0);
	}
	if(changes & 0x08) set_is_double_density(!(control & 0x08));
}

void Plus3::set_motor_on(bool on) {
	// TODO: this status should transfer if the selected drive changes. But the same goes for
	// writing state, so plenty of work to do in general here.
	get_drive().set_motor_on(on);
}
