//
//  ADB.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "ADB.hpp"

#include <cstdio>

using namespace Apple::IIgs::ADB;

uint8_t GLU::get_keyboard_data() {
	return 0;
}

uint8_t GLU::get_mouse_data() {
	return 0x80;
}

uint8_t GLU::get_modifier_status() {
	return 0x00;
}

uint8_t GLU::get_data() {
	return 0x00;
}

uint8_t GLU::get_status() {
	return 0x00;
}

void GLU::set_command(uint8_t) {
	printf("TODO: set ADB command\n");
}

void GLU::set_status(uint8_t) {
	printf("TODO: set ADB status\n");
}
