//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

#include "../../Outputs/Log.hpp"

#include <algorithm>

using namespace Atari::ST;

namespace {

struct ModeParams {
	const int lines_per_frame;

	const int first_video_line;
	const int final_video_line;

	const int line_length;

	const int end_of_blank;

	const int start_of_display_enable;
	const int end_of_display_enable;

	const int start_of_output;
	const int end_of_output;

	const int start_of_blank;

	const int start_of_hsync;
	const int end_of_hsync;
} modes[3] = {
	{313,	56, 256,	1024,		64,		116, 116+640,	116+48, 116+48+640,		904,	928, 1008	},
	{},
	{}
};

const ModeParams &mode_params_for_mode() {
	// TODO: rest of potential combinations, and accept mode as a paramter.
	return modes[0];
}

}

Video::Video() :
	crt_(1024, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4) {
}

void Video::set_ram(uint16_t *ram) {
	ram_ = ram;
}

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

void Video::run_for(HalfCycles duration) {
	int integer_duration = int(duration.as_integral());
	const auto mode_params = mode_params_for_mode();

#define Period(lower, upper, type)	\
	if(x >= lower && x < upper) {	\
		const auto target = std::min(upper, final_x);	\
		type(target - x);	\
		x = target;	\
	}

	// TODO: the below is **way off**. The real hardware does what you'd expect with ongoing state and
	// exact equality tests. Fixes to come.

	while(integer_duration) {
		const int final_x = std::min(x + integer_duration, mode_params.line_length);
		integer_duration -= (final_x - x);

		if(y >= mode_params.first_video_line && y < mode_params.final_video_line) {
			// TODO: Prior to output: collect all necessary data, obeying start_of_display_enable and end_of_display_enable.

			Period(0, 							mode_params.end_of_blank, 		crt_.output_blank);
			Period(mode_params.end_of_blank, 	mode_params.start_of_output, 	output_border);

			if(x >= mode_params.start_of_output && x < mode_params.end_of_output) {
				if(x == mode_params.start_of_output) {
					// TODO: resolutions other than 320.
					pixel_pointer_ = reinterpret_cast<uint16_t *>(crt_.begin_data(320));
				}

				const auto target = std::min(mode_params.end_of_output, final_x);
				while(x < target) {
					if(!(x&31) && pixel_pointer_) {
						// TODO: RAM sizes other than 512kb.
						uint16_t source[4] = {
							ram_[(current_address_ + 0) & 262143],
							ram_[(current_address_ + 1) & 262143],
							ram_[(current_address_ + 2) & 262143],
							ram_[(current_address_ + 3) & 262143],
						};
						current_address_ += 4;

						for(int c = 0; c < 16; ++c) {
							*pixel_pointer_ = palette_[
								((source[0] >> 12) & 0x8)	|
								((source[1] >> 13) & 0x4)	|
								((source[2] >> 14) & 0x2)	|
								((source[3] >> 15) & 0x1)
							];
							source[0] <<= 1;
							source[1] <<= 1;
							source[2] <<= 1;
							source[3] <<= 1;
							++pixel_pointer_;
						}
					}
					++x;
				}

				if(x == mode_params.end_of_output) {
					crt_.output_data(mode_params.end_of_output - mode_params.start_of_output, 320);
					pixel_pointer_ = nullptr;
				}
			}

			Period(mode_params.end_of_output, 	mode_params.start_of_blank, 	output_border);
			Period(mode_params.start_of_blank, 	mode_params.start_of_hsync,		crt_.output_blank);
			Period(mode_params.start_of_hsync,	mode_params.end_of_hsync,		crt_.output_sync);
			Period(mode_params.end_of_hsync, 	mode_params.line_length,		crt_.output_blank);
		} else {
			// Hard code the first three lines as vertical sync.
			if(y < 3) {
				Period(0,							mode_params.start_of_hsync,	crt_.output_sync);
				Period(mode_params.start_of_hsync,	mode_params.end_of_hsync,	crt_.output_blank);
				Period(mode_params.end_of_hsync,	mode_params.line_length,	crt_.output_sync);
			} else {
				Period(0, 							mode_params.end_of_blank, 		crt_.output_blank);
				Period(mode_params.end_of_blank, 	mode_params.start_of_blank, 	output_border);
				Period(mode_params.start_of_blank, 	mode_params.start_of_hsync,		crt_.output_blank);
				Period(mode_params.start_of_hsync,	mode_params.end_of_hsync,		crt_.output_sync);
				Period(mode_params.end_of_hsync, 	mode_params.line_length,		crt_.output_blank);
			}
		}

		if(x == mode_params.line_length) {
			x = 0;
			y = (y + 1) % mode_params.lines_per_frame;
			if(!y)
				current_address_ = base_address_ >> 1;
		}
	}

#undef Period
}

void Video::output_border(int duration) {
	uint16_t *colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
	if(colour_pointer) *colour_pointer = palette_[0];
	crt_.output_level(duration);
}

bool Video::hsync() {
	const auto mode_params = mode_params_for_mode();
	return x >= mode_params.start_of_hsync && x < mode_params.end_of_hsync;
}

bool Video::vsync() {
	return y < 3;
}

bool Video::display_enabled() {
	const auto mode_params = mode_params_for_mode();
	return y >= mode_params.first_video_line && y < mode_params.final_video_line && x >= mode_params.start_of_display_enable && x < mode_params.end_of_display_enable;
}

HalfCycles Video::get_next_sequence_point() {
	// The next hsync transition will occur either this line or the next.
	const auto mode_params = mode_params_for_mode();
	HalfCycles cycles_until_hsync;
	if(x < mode_params.start_of_hsync) {
		cycles_until_hsync = HalfCycles(mode_params.start_of_hsync - x);
	} else if(x < mode_params.end_of_hsync) {
		cycles_until_hsync = HalfCycles(mode_params.end_of_hsync - x);
	} else {
		cycles_until_hsync = HalfCycles(mode_params.start_of_hsync + mode_params.line_length - x);
	}

	// The next vsync transition depends purely on the current y.
	HalfCycles cycles_until_vsync;
	if(y < 3) {
		cycles_until_vsync = HalfCycles(mode_params.line_length - x + (2 - y)*mode_params.line_length);
	} else {
		cycles_until_vsync = HalfCycles(mode_params.line_length - x + (mode_params.lines_per_frame - 1 - y)*mode_params.line_length);
	}

	// The next display enable transition will occur only in the visible area.
	HalfCycles cycles_until_display_enable;
	if(display_enabled()) {
		cycles_until_display_enable = HalfCycles(mode_params.end_of_display_enable - x);
	} else {
		const auto horizontal_cycles = mode_params.start_of_display_enable - x;
		int vertical_lines = 0;
		if(y < mode_params.first_video_line) {
			vertical_lines = mode_params.first_video_line - y;
		} else if(y >= mode_params.final_video_line ) {
			vertical_lines = mode_params.first_video_line + mode_params.lines_per_frame - y;
		}
		if(horizontal_cycles < 0) ++vertical_lines;
		cycles_until_display_enable = HalfCycles(horizontal_cycles + vertical_lines * mode_params.line_length);
	}

	// Determine the minimum of the three
	if(cycles_until_hsync < cycles_until_vsync && cycles_until_hsync < cycles_until_display_enable) {
		return cycles_until_hsync;
	} else {
		return (cycles_until_vsync < cycles_until_display_enable) ? cycles_until_vsync : cycles_until_display_enable;
	}
}

// MARK: - IO dispatch

uint16_t Video::read(int address) {
	LOG("[Video] read " << PADHEX(2) << (address & 0x3f));
	address &= 0x3f;
	switch(address) {
		default:
		break;
		case 0x00:	return uint16_t(0xff00 | (base_address_ >> 16));
		case 0x01:	return uint16_t(0xff00 | (base_address_ >> 8));
		case 0x02:	return uint16_t(0xff00 | (current_address_ >> 16));
		case 0x03:	return uint16_t(0xff00 | (current_address_ >> 8));
		case 0x04:	return uint16_t(0xff00 | (current_address_));
		case 0x30:	return video_mode_ | 0xfcff;
	}
	return 0xff;
}

void Video::write(int address, uint16_t value) {
	LOG("[Video] write " << PADHEX(2) << int(value) << " to " << PADHEX(2) << (address & 0x3f));
	address &= 0x3f;
	switch(address) {
		default: break;

		// Start address.
		case 0x00:	base_address_ = (base_address_ & 0x00ffff) | ((value & 0xff) << 16);	break;
		case 0x01:	base_address_ = (base_address_ & 0xff00ff) | ((value & 0xff) << 8);		break;

		// Mode.
		case 0x30:	video_mode_ = value;	break;

		// Palette.
		case 0x20:	case 0x21:	case 0x22:	case 0x23:
		case 0x24:	case 0x25:	case 0x26:	case 0x27:
		case 0x28:	case 0x29:	case 0x2a:	case 0x2b:
		case 0x2c:	case 0x2d:	case 0x2e:	case 0x2f: {
			uint8_t *const entry = reinterpret_cast<uint8_t *>(&palette_[address - 0x20]);
			entry[0] = uint8_t((value & 0x700) >> 7);
			entry[1] = uint8_t((value & 0x77) << 1);
		} break;
	}
}
