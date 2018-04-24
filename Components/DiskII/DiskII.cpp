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
}

void DiskII::select_drive(int drive) {
	printf("Select drive %d\n", drive);
}

void DiskII::set_shift_register(uint8_t value) {
	printf("Set shift register\n");
}

uint8_t DiskII::get_shift_register() {
	printf("Get shift register\n");
	return 0xff;
}

void DiskII::run_for(const Cycles cycles) {
}
