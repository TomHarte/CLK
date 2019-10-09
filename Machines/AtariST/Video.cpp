//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

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

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

void Video::run_for(HalfCycles duration) {
	int integer_duration = duration.as_int();
	const auto mode_params = mode_params_for_mode();

#define Period(lower, upper, type)	\
	if(x >= lower && x < upper) {	\
		const auto target = std::min(upper, final_x);	\
		type(target - x);	\
		x = target;	\
	}

	while(integer_duration) {
		const int final_x = std::min(x + integer_duration, mode_params.line_length);
		integer_duration -= (final_x - x);

		if(y >= mode_params.first_video_line && y < mode_params.final_video_line) {
			// TODO: Prior to output: collect all necessary data, obeying start_of_display_enable and end_of_display_enable.

			Period(0, 							mode_params.end_of_blank, 		crt_.output_blank);
			Period(mode_params.end_of_blank, 	mode_params.start_of_output, 	output_border);

			if(x >= mode_params.start_of_output && x < mode_params.end_of_output) {
				const auto target = std::min(mode_params.end_of_output, final_x);
				x = target;

				if(x == mode_params.end_of_output) {
					uint16_t *colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
					if(colour_pointer) *colour_pointer = 0xffff;
					crt_.output_level(mode_params.end_of_output - mode_params.start_of_output);
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
		}
	}

#undef Period
}

void Video::output_border(int duration) {
	uint16_t *colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
	if(colour_pointer) *colour_pointer = 0x333;
	crt_.output_level(duration);
}
