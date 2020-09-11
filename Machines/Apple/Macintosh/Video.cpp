//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

#include <algorithm>

using namespace Apple::Macintosh;

// Re: CRT timings, see the Apple Guide to the Macintosh Hardware Family,
// bottom of page 400:
//
//	"For each scan line, 512 pixels are drawn on the screen ...
//	The horizontal blanking interval takes the time of an additional 192 pixels"
//
// And, at the top of 401:
//
//	"The visible portion of a full-screen display consists of 342 horizontal scan lines...
//	During the vertical blanking interval, the turned-off beam ... traces out an additional 28 scan lines,"
//
Video::Video(DeferredAudio &audio, DriveSpeedAccumulator &drive_speed_accumulator) :
	audio_(audio),
	drive_speed_accumulator_(drive_speed_accumulator),
 	crt_(704, 1, 370, 6, Outputs::Display::InputDataType::Luminance1) {

 	crt_.set_display_type(Outputs::Display::DisplayType::RGB);

	// UGLY HACK. UGLY, UGLY HACK. UGLY!
	// The OpenGL scan target fails properly to place visible areas which are not 4:3.
	// The [newer] Metal scan target has no such issue. So assume that Apple => Metal,
	// and set a visible area to work around the OpenGL issue if required.
	// TODO: eliminate UGLY HACK.
#ifdef __APPLE__
	crt_.set_visible_area(Outputs::Display::Rect(0.08f, 10.0f / 368.0f, 0.82f, 344.0f / 368.0f));
#else
	crt_.set_visible_area(Outputs::Display::Rect(0.08f, -0.025f, 0.82f, 0.82f));
#endif
	crt_.set_aspect_ratio(1.73f);	// The Mac uses a non-standard scanning area.
}

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Video::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status() / 2.0f;
}

void Video::run_for(HalfCycles duration) {
	// Determine the current video and audio bases. These values don't appear to be latched, they apply immediately.
	const size_t video_base = (use_alternate_screen_buffer_ ? (0xffff2700 >> 1) : (0xffffa700 >> 1)) & ram_mask_;
	const size_t audio_base = (use_alternate_audio_buffer_ ? (0xffffa100 >> 1) : (0xfffffd00 >> 1)) & ram_mask_;

	// The number of HalfCycles is literally the number of pixel clocks to move through,
	// since pixel output occurs at twice the processor clock. So divide by 16 to get
	// the number of fetches.
	while(duration > HalfCycles(0)) {
		const auto pixel_start = frame_position_ % line_length;
		const int line = int((frame_position_ / line_length).as_integral());

		const auto cycles_left_in_line = std::min(line_length - pixel_start, duration);

		// Line timing, entirely invented as I can find exactly zero words of documentation:
		//
		//	First 342 lines:
		//
		//	First 32 words = pixels;
		//	next 5 words = right border;
		//	next 2 words = sync level;
		//	final 5 words = left border.
		//
		//	Then 12 lines of border, 3 of sync, 11 more of border.

		const int first_word = int(pixel_start.as_integral()) >> 4;
		const int final_word = int((pixel_start + cycles_left_in_line).as_integral()) >> 4;

		if(first_word != final_word) {
			if(line < 342) {
				// If there are any pixels left to output, do so.
				if(first_word < 32) {
					const int final_pixel_word = std::min(final_word, 32);

					if(!first_word) {
						pixel_buffer_ = crt_.begin_data(512);
					}

					if(pixel_buffer_) {
						for(int c = first_word; c < final_pixel_word; ++c) {
							uint16_t pixels = ram_[video_base + video_address_] ^ 0xffff;
							++video_address_;

							pixel_buffer_[15] = pixels & 0x01;
							pixel_buffer_[14] = pixels & 0x02;
							pixel_buffer_[13] = pixels & 0x04;
							pixel_buffer_[12] = pixels & 0x08;
							pixel_buffer_[11] = pixels & 0x10;
							pixel_buffer_[10] = pixels & 0x20;
							pixel_buffer_[9] = pixels & 0x40;
							pixel_buffer_[8] = pixels & 0x80;

							pixels >>= 8;
							pixel_buffer_[7] = pixels & 0x01;
							pixel_buffer_[6] = pixels & 0x02;
							pixel_buffer_[5] = pixels & 0x04;
							pixel_buffer_[4] = pixels & 0x08;
							pixel_buffer_[3] = pixels & 0x10;
							pixel_buffer_[2] = pixels & 0x20;
							pixel_buffer_[1] = pixels & 0x40;
							pixel_buffer_[0] = pixels & 0x80;

							pixel_buffer_ += 16;
						}
					}

					if(final_pixel_word == 32) {
						crt_.output_data(512);
						pixel_buffer_ = nullptr;
					}
				}

				if(first_word < sync_start && final_word >= sync_start)	crt_.output_blank((sync_start - 32) * 16);
				if(first_word < sync_end && final_word >= sync_end)		crt_.output_sync((sync_end - sync_start) * 16);
				if(final_word == 44)									crt_.output_blank((44 - sync_end) * 16);
			} else if(final_word == 44) {
				if(line >= 353 && line < 356) {
					/* Output a sync line. */
					crt_.output_sync(sync_start * 16);
					crt_.output_blank((sync_end - sync_start) * 16);
					crt_.output_sync((44 - sync_end) * 16);
				} else {
					/* Output a blank line. */
					crt_.output_blank(sync_start * 16);
					crt_.output_sync((sync_end - sync_start) * 16);
					crt_.output_blank((44 - sync_end) * 16);
				}
			}

			// Audio and disk fetches occur "just before video data".
			if(final_word == 44) {
				const uint16_t audio_word = ram_[audio_address_ + audio_base];
				++audio_address_;
				audio_.audio.post_sample(audio_word >> 8);
				drive_speed_accumulator_.post_sample(audio_word & 0xff);
			}
		}

		duration -= cycles_left_in_line;
		frame_position_ = frame_position_ + cycles_left_in_line;
		if(frame_position_ == frame_length) {
			frame_position_ = HalfCycles(0);
			/*
				Video: $1A700 and the alternate buffer starts at $12700; for a 512K Macintosh, add $60000 to these numbers.
			*/
			video_address_ = 0;

			/*
				"The main sound buffer is at $1FD00 in a 128K Macintosh, and the alternate buffer is at $1A100;
				for a 512K Macintosh, add $60000 to these values."
			*/
			audio_address_ = 0;
		}
	}
}

bool Video::vsync() {
	const auto line = (frame_position_ / line_length).as_integral();
	return line >= 353 && line < 356;
}

HalfCycles Video::get_next_sequence_point() {
	const auto line = (frame_position_ / line_length).as_integral();
	if(line >= 353 && line < 356) {
		// Currently in vsync, so get time until start of line 357,
		// when vsync will end.
		return HalfCycles(356) * line_length - frame_position_;
	} else {
		// Not currently in vsync, so get time until start of line 353.
		const auto start_of_vsync = HalfCycles(353) * line_length;
		if(frame_position_ < start_of_vsync)
			return start_of_vsync - frame_position_;
		else
			return start_of_vsync + HalfCycles(number_of_lines) * line_length - frame_position_;
	}
}

void Video::set_use_alternate_buffers(bool use_alternate_screen_buffer, bool use_alternate_audio_buffer) {
	use_alternate_screen_buffer_ = use_alternate_screen_buffer;
	use_alternate_audio_buffer_ = use_alternate_audio_buffer;
}

void Video::set_ram(uint16_t *ram, uint32_t mask) {
	ram_ = ram;
	ram_mask_ = mask;
}
