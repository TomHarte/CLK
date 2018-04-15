//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

using namespace AppleII;

Video::Video() :
	crt_(new Outputs::CRT::CRT(455, 1, Outputs::CRT::DisplayType::NTSC60, 1)) {

	// Set a composite sampling function that assumes 1bpp input, and uses just 7 bits per byte.
	crt_->set_composite_sampling_function(
		"float composite_sample(usampler2D sampler, vec2 coordinate, vec2 icoordinate, float phase, float amplitude)"
		"{"
			"uint texValue = texture(sampler, coordinate).r;"
			"texValue <<= int(icoordinate.x * 8) & 7;"
			"return float(texValue & 128u);"
//			"uint texValue = texture(sampler, coordinate).r;"
//			"texValue <<= uint(icoordinate.x * 7.0) % 7u;"
//			"return float(texValue & 128u);"
		"}");

	// Show only the centre 75% of the TV frame.
	crt_->set_video_signal(Outputs::CRT::VideoSignal::Composite);
	crt_->set_visible_area(Outputs::CRT::Rect(0.115f, 0.115f, 0.77f, 0.77f));
}

Outputs::CRT::CRT *Video::get_crt() {
	return crt_.get();
}

void Video::run_for(const Cycles cycles) {
	/*
		Addressing scheme used throughout is that column 0 is the first column with pixels in it;
		row 0 is the first row with pixels in it.

		A frame is oriented around 65 cycles across, 262 lines down.
	*/
	const int first_sync_line = 220;	// A complete guess. Information needed.
	const int first_sync_column = 49;	// Also a guess.

	int int_cycles = cycles.as_int();
	while(int_cycles) {
		const int cycles_this_line = std::min(65 - column_, int_cycles);

		if(row_ >= first_sync_line && row_ < first_sync_line + 3) {
			crt_->output_sync(static_cast<unsigned int>(cycles_this_line) * 7);
		} else {
			const int ending_column = column_ + cycles_this_line;

			// The first 40 columns are submitted to the CRT only upon completion;
			// they'll be either graphics or blank, depending on which side we are
			// of line 192.
			if(column_ < 40) {
				if(row_ < 192) {
					if(!column_) {
						pixel_pointer_ = crt_->allocate_write_area(40);
					}

					// TODO: actually store pixels.

					if(ending_column >= 40) {
						for(int c = 0; c < 40; ++c) {
							pixel_pointer_[c] = static_cast<uint8_t>((c * 6) ^ row_);
						}
						crt_->output_data(280, 7);
					}
				} else {
					if(ending_column >= 40) {
						crt_->output_blank(280);
					}
				}
			}

			/*
				The left border, sync, right border pattern doesn't depend on whether
				there were pixels this row and is output as soon as it is known.
			*/

			const int first_blank_start = std::max(40, column_);
			const int first_blank_end = std::min(first_sync_column, ending_column);
			if(first_blank_end > first_blank_start) {
				crt_->output_blank(static_cast<unsigned int>(first_blank_end - first_blank_start) * 7);
			}

			// TODO: colour burst.

			const int sync_start = std::max(first_sync_column, column_);
			const int sync_end = std::min(first_sync_column + 4, ending_column);
			if(sync_end > sync_start) {
				crt_->output_sync(static_cast<unsigned int>(sync_end - sync_start) * 7);
			}

			const int second_blank_start = std::max(first_sync_column + 4, column_);
			if(ending_column > second_blank_start) {
				crt_->output_blank(static_cast<unsigned int>(ending_column - second_blank_start) * 7);
			}
		}

		int_cycles -= cycles_this_line;
		column_ = (column_ + cycles_this_line) % 65;
		if(!column_) {
			row_ = (row_ + 1) % 262;

			// Add an extra half a colour cycle of blank; this isn't counted in the run_for
			// count explicitly but is promised.
			crt_->output_blank(1);
		}
	}
}

void Video::set_graphics_mode() {
	printf("Graphics mode\n");
}

void Video::set_text_mode() {
	printf("Text mode\n");
}

void Video::set_mixed_mode(bool mixed_mode) {
	printf("Mixed mode: %s\n", mixed_mode ? "true" : "false");
}

void Video::set_video_page(int page) {
	printf("Video page: %d\n", page);
}

void Video::set_low_resolution() {
	printf("Low resolution\n");
}

void Video::set_high_resolution() {
	printf("High resolution\n");
}
