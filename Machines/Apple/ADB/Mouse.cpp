//
//  Mouse.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Mouse.hpp"

#include <algorithm>

using namespace Apple::ADB;

Mouse::Mouse(Bus &bus) : ReactiveDevice(bus, 3) {}

void Mouse::perform_command(const Command &command) {
	// Mouse deltas are confined to a seven-bit signed field; this implementation keeps things symmetrical by
	// limiting them to a maximum absolute value of 63 in any direction.
	static constexpr int16_t max_delta = 63;

	if(command.type == Command::Type::Talk && command.reg == 0) {
		// Read and clamp current deltas and buttons.
		//
		// There's some small chance of creating negative feedback here — taking too much off delta_x_ or delta_y_
		// due to a change in the underlying value between the load and the arithmetic. But if that occurs it means
		// the user moved the mouse again in the interim, so it'll just play out as very slight latency.
		auto delta_x = delta_x_.load(std::memory_order_relaxed);
		auto delta_y = delta_y_.load(std::memory_order_relaxed);
		if(abs(delta_x) > max_delta || abs(delta_y) > max_delta) {
			int max = std::max(abs(delta_x), abs(delta_y));
			delta_x = delta_x * max_delta / max;
			delta_y = delta_y * max_delta / max;
		}

		const int buttons = button_flags_.load(std::memory_order_relaxed);
		delta_x_ -= delta_x;
		delta_y_ -= delta_y;

		// Figure out what that would look like, and don't respond if there's
		// no change or deltas to report.
		const uint16_t reg0 =
			((buttons & 1) ? 0x0000 : 0x8000) |
			((buttons & 2) ? 0x0000 : 0x0080) |
			uint16_t(delta_x & 0x7f) |
			uint16_t((delta_y & 0x7f) << 8);
		if(!(reg0 & 0x7f7f) && (reg0 & 0x8080) == (last_posted_reg0_ & 0x8080)) return;

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
