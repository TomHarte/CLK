//
//  Nick.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Nick.hpp"

#include <cstdio>

namespace  {

uint16_t mapped_colour(uint8_t source) {
	// On the Enterprise, red and green are 3-bit quantities; blue is a 2-bit quantity.
	const int red	= ((source&0x01) << 2) | ((source&0x08) >> 2) | ((source&0x40) >> 6);
	const int green	= ((source&0x02) << 1) | ((source&0x10) >> 3) | ((source&0x80) >> 7);
	const int blue	= ((source&0x04) >> 1) | ((source&0x20) >> 5);

	// Duplicate bits where necessary to map to a full 4-bit range per channel.
	return uint16_t(
		(red << 9) + ((red&0x4) << 6) +
		(green << 5) + ((green&0x4) << 2) +
		(blue << 2) + blue
	);
}

}

using namespace Enterprise;

Nick::Nick(const uint8_t *ram) :
	crt_(57, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4),
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

		case 1:
			flush_border();
			border_colour_ = mapped_colour(value);
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

			// Special: set mode as soon as it's known. It'll be needed at the end of HSYNC.
			if(window < 2 && fetch_spot >= 2) {
				// Set the output mode and margin.
				left_margin_ = line_parameters_[2] & 0x3f;
				right_margin_ = line_parameters_[3] & 0x3f;
				mode_ = Mode((line_parameters_[1] >> 1)&7);

				// Act as if proper state transitions had occurred while HSYNC is being output.
				if(mode_ == Mode::Vsync) {
					state_ = State::Blank;
				} else {
					// The first ten windows are occupied by the horizontal sync and
					// colour burst; if left signalled before then, begin in pixels.
					state_ = left_margin_ > 10 ? State::Border : State::Pixels;
				}
			}

			// If all parameters have been loaded, set appropriate fields.
			if(fetch_spot == 8) {
				should_reload_line_parameters_ = false;

				// Set length of mode line.
				lines_remaining_ = line_parameters_[0];

				// Determine the line data pointers.
				line_data_pointer_[0] = uint16_t(line_parameters_[4] | (line_parameters_[5] << 8));
				line_data_pointer_[1] = uint16_t(line_parameters_[6] | (line_parameters_[7] << 8));
			}
		}

		// HSYNC is signalled for four windows at the start of the line.
		// I currently belive this happens regardless of Vsync mode.
		if(window < 4 && end_window >= 4) {
			crt_.output_sync(4);
			window = 4;
		}

		// Deal with vsync mode out here.
		if(mode_ == Mode::Vsync) {
			if(window >= 4) {
				while(window < end_window) {
					int next_event = end_window;
					if(window < left_margin_) next_event = std::min(next_event, left_margin_);
					if(window < right_margin_) next_event = std::min(next_event, right_margin_);

					if(state_ == State::Blank) {
						crt_.output_blank(next_event - window);
					} else {
						crt_.output_sync(next_event - window);
					}

					window = next_event;
					if(window == left_margin_) state_ = State::Sync;
					if(window == right_margin_) state_ = State::Blank;
				}
			}
		} else {
			// If present then the colour burst is output for the period from
			// the start of window 6 to the end of window 10.
			if(window < 10 && end_window >= 10) {
				crt_.output_blank(2);
				crt_.output_colour_burst(4, 0);	// TODO: try to determine actual phase.
				window = 10;
			}

			if(window >= 10) {
				while(window < end_window) {
					int next_event = end_window;
					if(window < left_margin_) next_event = std::min(next_event, left_margin_);
					if(window < right_margin_) next_event = std::min(next_event, right_margin_);

					if(state_ == State::Border) {
						border_duration_ += next_event - window;
					} else {
						if(!allocated_pointer_) {
							flush_pixels();
							pixel_pointer_ = allocated_pointer_ = reinterpret_cast<uint16_t *>(crt_.begin_data(allocation_size));
						}

						// TODO: real pixels.
						if(allocated_pointer_) {
							for(int c = 0; c < next_event - window; c++) {
								pixel_pointer_[0] = uint16_t(0xfff ^ (window + c));
								++pixel_pointer_;
							}
						} else {
							pixel_pointer_ += next_event - window;
						}

						pixel_duration_ += next_event - window;
						if(pixel_pointer_ - allocated_pointer_ == allocation_size) {
							flush_pixels();
						}
					}

					window = next_event;
					if(window == left_margin_) {
						flush_border();
						state_ = State::Pixels;
					}
					if(window == right_margin_) {
						flush_pixels();
						state_ = State::Border;
					}
				}
			}

			// Finish up the line.
			if(!horizontal_counter_) {
				if(state_ == State::Border) {
					flush_border();
				} else {
					flush_pixels();
				}
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

			// TODO: should reload line data pointers?
		}
	}
}

void Nick::flush_border() {
	if(!border_duration_) return;

	uint16_t *const colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
	if(colour_pointer) *colour_pointer = border_colour_;
	crt_.output_level(border_duration_);
	border_duration_ = 0;
}

void Nick::flush_pixels() {
	if(!pixel_duration_) return;
	crt_.output_data(pixel_duration_, size_t(pixel_pointer_ - allocated_pointer_));
	pixel_duration_ = 0;
	pixel_pointer_ = nullptr;
	allocated_pointer_ = nullptr;
}

// MARK: - CRT passthroughs.

void Nick::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Nick::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status();
}
