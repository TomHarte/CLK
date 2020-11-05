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
	// The classic Apple II serial keyboard register:
	// b7:		key strobe.
	// b6–b0:	ASCII code.
	return 0x00;
}

void GLU::clear_key_strobe() {
	// Clears the key strobe of the classic Apple II serial keyboard register.
}

uint8_t GLU::get_any_key_down() {
	// The Apple IIe check-for-any-key-down bit.
	return 0x00;
}

uint8_t GLU::get_mouse_data() {
	// Alternates between returning x and y values.
	//
	// b7: 		1 = button is up; 0 = button is down.
	// b6:		delta sign bit; 1 = negative.
	// b5–b0:	mouse delta.
	return 0x80;
}

uint8_t GLU::get_modifier_status() {
	// b7:		1 = command key pressed; 0 = not.
	// b6:		option key.
	// b5:		1 = modifier key latch has been updated, no key has been pressed; 0 = not.
	// b4:		any numeric keypad key.
	// b3:		a key is down.
	// b2:		caps lock is pressed.
	// b1:		control key.
	// b0:		shift key.
	return 0x00;
}

uint8_t GLU::get_data() {
	// b0–2:	number of data bytes to be returned.
	// b3:		1 = a valid service request is pending; 0 = no request pending.
	// b4:		1 = control, command and delete keys have been pressed simultaneously; 0 = they haven't.
	// b5:		1 = control, command and reset have all been pressed together; 0 = they haven't.
	// b6:		1 = ADB controller encountered an error and reset itself; 0 = no error.
	// b7:		1 = ADB has received a response from the addressed ADB device; 0 = no respone.
	return 0x00;
}

uint8_t GLU::get_status() {
	// b7:	1 = mouse data register is full; 0 = empty.
	// b6:	1 = mouse interrupt is enabled.
	// b5:	1 = command/data has valid data.
	// b4:	1 = command/data interrupt is enabled.
	// b3:	1 = keyboard data is full.
	// b2:	1 = keyboard data interrupt is enabled.
	// b1:	1 = mouse x-data is available; 0 = y.
	// b0:	1 = command register is full (set when command is written); 0 = empty (cleared when data is read).
	return 0x00;
}

void GLU::set_command(uint8_t command) {
	printf("TODO: set ADB command: %02x\n", command);
}

void GLU::set_status(uint8_t status) {
	printf("TODO: set ADB status %02x\n", status);
}
