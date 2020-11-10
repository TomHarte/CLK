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

// MARK: - Configuration.

void GLU::set_is_rom03(bool is_rom03) {
	is_rom03_ = is_rom03;
}

// MARK: - External interface.

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
	printf("ADB get data\n");
	if(!pending_response_.empty()) {
		// A bit yucky, pull from the from the front.
		// This keeps my life simple for now, even though it's inefficient.
		const uint8_t next = pending_response_[0];
		pending_response_.erase(pending_response_.begin());
		return next;
	}

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
	const uint8_t status =
		(pending_response_.empty() ? 0 : 0x20);	// Data is valid if a response is pending.
//	printf("ADB get status : %02x\n", status);
	return status;
}

void GLU::set_command(uint8_t command) {
	// Accumulate input until a full comamnd is received;
	// the state machine otherwise gets somewhat oversized.
	next_command_.push_back(command);

	// Command dispatch; each entry should do whatever it needs
	// to do and then either: (i) break, if completed; or
	// (ii) return, if awaiting further input.
#define RequireSize(n)	if(next_command_.size() < (n)) return;
	switch(next_command_[0]) {
		case 0x01:	abort();					break;
		case 0x02:	reset_microcontroller();	break;
		case 0x03:	flush_keyboard_buffer();	break;

		case 0x04:	// Set modes.
			RequireSize(2);
			set_modes(modes_ | next_command_[1]);
		break;

		case 0x05:	// Clear modes.
			RequireSize(2);
			set_modes(modes_ & ~next_command_[1]);
		break;

		case 0x06:	// Set configuration bytes
			RequireSize(is_rom03_ ? 8 : 4);
			set_configuration_bytes(&next_command_[1]);
		break;

		case 0x07:	// Sync.
			RequireSize(is_rom03_ ? 9 : 5);
			set_modes(next_command_[1]);
			set_configuration_bytes(&next_command_[2]);

			// ROM03 seems to expect to send an extra four bytes here,
			// with values 0x20, 0x26, 0x2d, 0x2d.
			// TODO: what does the ROM03-paired ADB GLU do with the extra four bytes?
		break;

		case 0x09:	// Read microcontroller memory.
			RequireSize(3);
			pending_response_.push_back(read_microcontroller_address(uint16_t(next_command_[1] | (next_command_[2] << 8))));
		break;

		case 0x0d:	// Get version number.
			pending_response_.push_back(6);	// Seems to be what ROM03 is looking for?
		break;

		// Enable device SRQ.
		case 0x50:	case 0x51:	case 0x52:	case 0x53:	case 0x54:	case 0x55:	case 0x56:	case 0x57:
		case 0x58:	case 0x59:	case 0x5a:	case 0x5b:	case 0x5c:	case 0x5d:	case 0x5e:	case 0x5f:
			set_device_srq(device_srq_ | (1 << (next_command_[1] & 0xf)));
		break;

		// Disable device SRQ.
		case 0x70:	case 0x71:	case 0x72:	case 0x73:	case 0x74:	case 0x75:	case 0x76:	case 0x77:
		case 0x78:	case 0x79:	case 0x7a:	case 0x7b:	case 0x7c:	case 0x7d:	case 0x7e:	case 0x7f:
			set_device_srq(device_srq_ & ~(1 << (next_command_[1] & 0xf)));
		break;

		default:
			printf("TODO: ADB command %02x\n", next_command_[0]);
		break;
	}
#undef RequireSize

	printf("ADB executed: ");
	for(auto c : next_command_) printf("%02x ", c);
	printf("\n");

	// Input was dealt with, so throw it away.
	next_command_.clear();
}

void GLU::set_status(uint8_t status) {
	printf("TODO: set ADB status %02x\n", status);
}

// MARK: - Internal commands.

void GLU::set_modes(uint8_t modes) {
	modes_ = modes;	// TODO: and this has what effect?
}

void GLU::abort() {
}

void GLU::reset_microcontroller() {
}

void GLU::flush_keyboard_buffer() {
}

void GLU::set_configuration_bytes(uint8_t *) {
}

void GLU::set_device_srq(int mask) {
	// TODO: do I need to do any more than this here?
	device_srq_ = mask;
}

uint8_t GLU::read_microcontroller_address(uint16_t address) {
	printf("Read from microcontroller %04x\n", address);
	return 0;
}
