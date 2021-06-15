//
//  Nick.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Nick.hpp"

#include <cstdio>

using namespace Enterprise;

Nick::Nick(const uint8_t *ram) :
	crt_(16 * 57, 16, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4),
	ram_(ram) {

	// Just use RGB for now.
	crt_.set_display_type(Outputs::Display::DisplayType::RGB);
}

void Nick::write(uint16_t address, uint8_t value) {
	printf("Nick write: %02x -> %d\n", value, address & 3);
	switch(address & 3) {
		default:
			printf("Unhandled\n");
		break;

		case 2:
			line_parameter_base_ = uint16_t((line_parameter_base_ & 0xf000) | (value << 4));
		break;
		case 3:
			line_parameter_base_ = uint16_t((line_parameter_base_ & 0x0ff0) | (value << 12));

			// Still a mystery to me: the exact meaning of the top two bits here. For now
			// just treat a 0 -> 1 transition of the MSB as a forced frame restart.
			if((value^line_parameter_control_) & value & 0x80) {
				printf("Should restart frame from %04x\n", line_parameter_base_);

				// For now: just force this to be the final line of this mode block.
				// I'm unclear whether I should also reset the horizontal counter
				// (i.e. completely abandon current video phase).
				lines_remaining_ = 0xff;
				line_parameters_[1] |= 1;
			}
			line_parameter_control_ = value & 0xc0;
		break;
	}
}

uint8_t Nick::read([[maybe_unused]] uint16_t address) {
	return 0xff;
}

void Nick::run_for(HalfCycles duration) {
	constexpr int line_length = 912;

	int clocks_remaining = duration.as<int>();
	while(clocks_remaining) {
		// Determine how many cycles are left this line.
		const int clocks_this_line = std::min(clocks_remaining, line_length - horizontal_counter_);

		// Convert that into a [start/current] and end window.
		int window = horizontal_counter_ >> 4;
		const int end_window = (horizontal_counter_ + clocks_this_line) >> 4;

		// Advance the line counters.
		clocks_remaining -= clocks_this_line;
		horizontal_counter_ = (horizontal_counter_ + clocks_this_line) % line_length;

		// Do nothing if a window boundary isn't crossed.
		if(window == end_window) continue;

		// If this is within the first 8 cycles of the line, [possibly] fetch
		// the relevant part of the line parameters.
		if(should_reload_line_parameters_ && window < 8) {
			int fetch_spot = window;
			while(fetch_spot < end_window && fetch_spot < 8) {
				line_parameters_[(fetch_spot << 1)] = ram_[line_parameter_pointer_];
				line_parameters_[(fetch_spot << 1) + 1] = ram_[line_parameter_pointer_ + 1];
				line_parameter_pointer_ += 2;
				++fetch_spot;
			}

			// If all parameters have been loaded, set appropriate fields.
			if(fetch_spot == 8) {
				should_reload_line_parameters_ = false;

				// Set length of mode line.
				lines_remaining_ = line_parameters_[0];
			}
		}

		// Check for end of line.
		if(!horizontal_counter_) {
			++lines_remaining_;
			if(!lines_remaining_) {
				should_reload_line_parameters_ = true;

				// Check for end-of-frame.
				if(line_parameters_[1] & 1) {
					line_parameter_pointer_ = line_parameter_base_;
				}
			}
		}
	}
}

// MARK: - CRT passthroughs.

void Nick::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Nick::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}
