//
//  MouseJoystick.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "MouseJoystick.hpp"

#include <algorithm>

using namespace Amiga;

// MARK: - Mouse.

int Mouse::get_number_of_buttons() {
	return 2;
}

void Mouse::set_button_pressed(int button, bool is_set) {
	switch(button) {
		case 0:
			cia_state_ = (cia_state_ &~ 0x40) | (is_set ? 0 : 0x40);
		break;
		default:
		break;
	}
}

uint8_t Mouse::get_cia_button() const {
	return cia_state_;
}

void Mouse::reset_all_buttons() {
	cia_state_ = 0xff;
}

void Mouse::move(int x, int y) {
	position_[0] += x;
	position_[1] += y;
}

uint16_t Mouse::get_position() {
	// The Amiga hardware retains only eight bits of position
	// for the mouse; its software polls frequently and maps
	// changes into a larger space.
	//
	// On modern computers with 5k+ displays and trackpads, it
	// proved empirically possible to overflow the hardware
	// counters more quickly than software would poll.
	//
	// Therefore the approach taken for mapping mouse motion
	// into the Amiga is to do it in steps of no greater than
	// [-128, +127], as per the below.
	const int pending[] = {
		position_[0], position_[1]
	};

	const int8_t change[] = {
		int8_t(std::clamp(pending[0], -128, 127)),
		int8_t(std::clamp(pending[1], -128, 127))
	};

	position_[0] -= change[0];
	position_[1] -= change[1];
	declared_position_[0] += change[0];
	declared_position_[1] += change[1];

	return uint16_t(
		(declared_position_[1] << 8) |
		declared_position_[0]
	);
}

// MARK: - Joystick.

// TODO: add second fire button.

Joystick::Joystick() :
	ConcreteJoystick({
						Input(Input::Up),
						Input(Input::Down),
						Input(Input::Left),
						Input(Input::Right),
						Input(Input::Fire, 0),
					}) {}

void Joystick::did_set_input(const Input &input, bool is_active) {
	// Accumulate state.
	inputs_[input.type] = is_active;

	// Determine what that does to the two position bits.
	const auto low =
		(inputs_[Input::Type::Down] ^ inputs_[Input::Type::Right]) |
		(inputs_[Input::Type::Right] << 1);
	const auto high =
		(inputs_[Input::Type::Up] ^ inputs_[Input::Type::Left]) |
		(inputs_[Input::Type::Left] << 1);

	// Ripple upwards if that affects the mouse position counters.
	const uint8_t previous_low = position_ & 3;
	uint8_t low_upper = (position_ >> 2) & 0x3f;
	const uint8_t previous_high = (position_ >> 8) & 3;
	uint8_t high_upper = (position_ >> 10) & 0x3f;

	if(!low && previous_low == 3) ++low_upper;
	if(!previous_low && low == 3) --low_upper;
	if(!high && previous_high == 3) ++high_upper;
	if(!previous_high && high == 3) --high_upper;

	position_ = uint16_t(
		low | ((low_upper & 0x3f) << 2) |
		(high << 8) | ((high_upper & 0x3f) << 10)
	);
}

uint16_t Joystick::get_position() {
	return position_;
}

uint8_t Joystick::get_cia_button() const {
	return inputs_[Input::Type::Fire] ? 0xbf : 0xff;
}
