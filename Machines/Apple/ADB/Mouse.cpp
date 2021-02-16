//
//  Mouse.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Mouse.hpp"

using namespace Apple::ADB;

Mouse::Mouse(Bus &bus) : ReactiveDevice(bus, 3) {}

void Mouse::perform_command(const Command &command) {
	if(command.type == Command::Type::Talk && command.reg == 0) {
		// Read current deltas and buttons, thread safely.
		auto delta_x = delta_x_.exchange(0);
		auto delta_y = delta_y_.exchange(0);
		const int buttons = button_flags_;

		// Clamp deltas.
		delta_x = std::max(std::min(delta_x, int16_t(127)), int16_t(-128));
		delta_y = std::max(std::min(delta_y, int16_t(127)), int16_t(-128));

		// Figure out what that would look like, and don't respond if there's
		// no change to report.
		const uint16_t reg0 =
			((buttons & 1) ? 0x0000 : 0x8000) |
			((buttons & 2) ? 0x0000 : 0x0080) |
			uint16_t(delta_x & 0x7f) |
			uint16_t((delta_y & 0x7f) << 8);
		if(reg0 == last_posted_reg0_) return;

		// Post change.
		last_posted_reg0_ = reg0;
		post_response({uint8_t(reg0 >> 8), uint8_t(reg0)});
	}
}

void Mouse::move(int x, int y) {
	delta_x_ += int16_t(x);
	delta_y_ += int16_t(y);
	post_service_request();
}

int Mouse::get_number_of_buttons() {
	return 2;
}

void Mouse::set_button_pressed(int index, bool is_pressed) {
	if(is_pressed)
		button_flags_ |= (1 << index);
	else
		button_flags_ &= ~(1 << index);
	post_service_request();
}

void Mouse::reset_all_buttons() {
	button_flags_ = 0;
	post_service_request();
}
