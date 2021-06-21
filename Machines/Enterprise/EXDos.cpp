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
//	emplace_drives(2, 8000000, 300, 2);
	set_control_register(0x00);
}

void EXDos::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive) {
}

void EXDos::set_control_register(uint8_t control) {
	last_control_ = control;
}

uint8_t EXDos::get_control_register() {
	return last_control_;
}

void EXDos::set_motor_on(bool on) {
}
