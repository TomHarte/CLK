//
//  Video.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/10/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "Video.hpp"

#include "../../../Outputs/Log.hpp"

#include <algorithm>
#include <cstring>

using namespace Atari::ST;

namespace {

/*!
	Defines the line counts at which mode-specific events will occur:
	vertical enable being set and being reset, and the line on which
	the frame will end.
*/
const struct VerticalParams {
	const int set_enable;
	const int reset_enable;
	const int height;
} vertical_params[3] = {
	{63, 263, 313},	// 47 rather than 63 on early machines.
	{34, 234, 263},
	{1, 401, 500}	// 72 Hz mode: who knows?
};

/// @returns The correct @c VerticalParams for output at @c frequency.
const VerticalParams &vertical_parameters(Video::FieldFrequency frequency) {
	return vertical_params[int(frequency)];
}


/*!
	Defines the horizontal counts at which mode-specific events will occur:
	horizontal enable being set and being reset, blank being set and reset, and the
	intended length of this ine.

	The caller should:

		* latch line length at cycle 54 (TODO: also for 72Hz mode?);
		* at (line length - 50), start sync and reset enable (usually for the second time);
		* at (line length - 10), disable sync.
*/
const struct HorizontalParams {
	const int set_enable;
	const int reset_enable;

	const int set_blank;
	const int reset_blank;

	const int length;
} horizontal_params[3] = {
	{56*2, 376*2,	450*2, 28*2,	512*2},
	{52*2, 372*2,	450*2, 24*2,	508*2},
	{4*2, 164*2,	184*2, 2*2,		224*2}
};

const HorizontalParams &horizontal_parameters(Video::FieldFrequency frequency) {
	return horizontal_params[int(frequency)];
}

#ifndef NDEBUG
struct Checker {
	Checker() {
		for(int c = 0; c < 3; ++c) {
			// Expected horizontal order of events: reset blank, enable display, disable display, enable blank (at least 50 before end of line), end of line
			const auto horizontal = horizontal_parameters(Video::FieldFrequency(c));
			assert(horizontal.reset_blank < horizontal.set_enable);
			assert(horizontal.set_enable < horizontal.reset_enable);
			assert(horizontal.reset_enable < horizontal.set_blank);
			assert(horizontal.set_blank+50 < horizontal.length);

			// Expected vertical order of events: reset blank, enable display, disable display, enable blank (at least 50 before end of line), end of line
			const auto vertical = vertical_parameters(Video::FieldFrequency(c));
			assert(vertical.set_enable < vertical.reset_enable);
			assert(vertical.reset_enable < vertical.height);
		}
	}
} checker;
#endif

}

Video::Video() :
	crt_(1024, 1, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4),
	shifter_(crt_, palette_) {

	// Show a total of 260 lines; a little short for PAL but a compromise between that and the ST's
	// usual output height of 200 lines.
	crt_.set_visible_area(crt_.get_rect_for_area(33, 260, 216, 850, 4.0f / 3.0f));
}

void Video::set_ram(uint16_t *ram, size_t size) {
	ram_ = ram;
}

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

void Video::run_for(HalfCycles duration) {
	const auto horizontal_timings = horizontal_parameters(field_frequency_);
	const auto vertical_timings = vertical_parameters(field_frequency_);
	int integer_duration = int(duration.as_integral());

	// Effect any changes in visible state out here; they're not relevant in the inner loop.
	if(!pending_events_.empty()) {
		auto erase_iterator = pending_events_.begin();
		int duration_remaining = integer_duration;
		while(erase_iterator != pending_events_.end()) {
			erase_iterator->delay -= duration_remaining;
			if(erase_iterator->delay < 0) {
				duration_remaining = -erase_iterator->delay;
				erase_iterator->apply(public_state_);
				++erase_iterator;
			} else {
				break;
			}
		}
		if(erase_iterator != pending_events_.begin()) {
			pending_events_.erase(pending_events_.begin(), erase_iterator);
		}
	}

	while(integer_duration) {
		// Seed next event to end of line.
		int next_event = line_length_;

		// Check the explicitly-placed events.
		if(horizontal_timings.reset_blank > x_)		next_event = std::min(next_event, horizontal_timings.reset_blank);
		if(horizontal_timings.set_blank > x_)		next_event = std::min(next_event, horizontal_timings.set_blank);
		if(horizontal_timings.reset_enable > x_)	next_event = std::min(next_event, horizontal_timings.reset_enable);
		if(horizontal_timings.set_enable > x_) 		next_event = std::min(next_event, horizontal_timings.set_enable);

		// Check for events that are relative to existing latched state.
		if(line_length_ - 50*2 > x_)				next_event = std::min(next_event, line_length_ - 50*2);
		if(line_length_ - 10*2 > x_)				next_event = std::min(next_event, line_length_ - 10*2);

		// Also, a vertical sync event might intercede.
		if(vertical_.sync_schedule != VerticalState::SyncSchedule::None && x_ < 30*2 && next_event >= 30*2) {
			next_event = 30*2;
		}

		// Determine current output mode and number of cycles to output for.
		const int run_length = std::min(integer_duration, next_event - x_);
		const bool display_enable = vertical_.enable && horizontal_.enable;

		if(horizontal_.sync || vertical_.sync) {
			shifter_.output_sync(run_length);
		} else if(horizontal_.blank || vertical_.blank) {
			shifter_.output_blank(run_length);
		} else if(!vertical_.enable || !horizontal_.enable) {
			shifter_.output_border(run_length, output_bpp_);
		} else {
			// There will be pixels this line, subject to the shifter pipeline.
			// Divide into 8-[half-]cycle windows; at the start of each window fetch a word,
			// and during the rest of the window, shift out.
			int start_column = x_ >> 3;
			const int end_column = (x_ + run_length) >> 3;

			// Rules obeyed below:
			//
			//	Video fetches occur as the first act of business in a column. Each
			//	fetch is then followed by 8 shift clocks. Whether or not the shifter
			//	was reloaded by the fetch depends on the FIFO.

			if(start_column == end_column) {
				shifter_.output_pixels(run_length, output_bpp_);
			} else {
				// Continue the current column if partway across.
				if(x_&7) {
					// If at least one column boundary is crossed, complete this column.
					shifter_.output_pixels(8 - (x_ & 7), output_bpp_);
					++start_column;	// This starts a new column, so latch a new word.
					latch_word();
				}

				// Run for all columns that have their starts in this time period.
				int complete_columns = end_column - start_column;
				while(complete_columns--) {
					shifter_.output_pixels(8, output_bpp_);
					latch_word();
				}

				// Output the start of the next column, if necessary.
				if((x_ + run_length) & 7) {
					shifter_.output_pixels((x_ + run_length) & 7, output_bpp_);
				}
			}
		}

		// Check for whether line length should have been latched during this run.
		if(x_ <= 54*2 && (x_ + run_length) > 54*2) line_length_ = horizontal_timings.length;

		// Make a decision about vertical state on cycle 502.
		if(x_ <= 502*2 && (x_ + run_length) > 502*2) {
			next_y_ = y_ + 1;
			next_vertical_ = vertical_;
			next_vertical_.sync_schedule = VerticalState::SyncSchedule::None;

			// Use vertical_parameters to get parameters for the current output frequency.
			if(next_y_ == vertical_timings.set_enable) {
				next_vertical_.enable = true;
			} else if(next_y_ == vertical_timings.reset_enable) {
				next_vertical_.enable = false;
			} else if(next_y_ == vertical_timings.height) {
				next_y_ = 0;
				next_vertical_.sync_schedule = VerticalState::SyncSchedule::Begin;
				current_address_ = base_address_ >> 1;
			} else if(next_y_ == 3) {
				next_vertical_.sync_schedule = VerticalState::SyncSchedule::End;
			}
		}

		// Apply the next event.
		x_ += run_length;
		integer_duration -= run_length;

		// Check horizontal events.
		if(horizontal_timings.reset_blank == x_)		horizontal_.blank = false;
		else if(horizontal_timings.set_blank == x_)		horizontal_.blank = true;
		else if(horizontal_timings.reset_enable == x_)	horizontal_.enable = false;
		else if(horizontal_timings.set_enable == x_) 	horizontal_.enable = true;
		else if(line_length_ - 50*2 == x_)				horizontal_.sync = true;
		else if(line_length_ - 10*2 == x_)				horizontal_.sync = false;

		// Check vertical events.
		if(vertical_.sync_schedule != VerticalState::SyncSchedule::None && x_ == 30*2) {
			vertical_.sync = vertical_.sync_schedule == VerticalState::SyncSchedule::Begin;
			vertical_.enable &= !vertical_.sync;
		}

		// Check whether the terminating event was end-of-line; if so then advance
		// the vertical bits of state.
		if(x_ == line_length_) {
			x_ = 0;
			vertical_ = next_vertical_;
			y_ = next_y_;
		}

		// Chuck any deferred output changes into the queue.
		const bool next_display_enable = vertical_.enable && horizontal_.enable;
		if(display_enable != next_display_enable) {
			add_event(28*2 - integer_duration, next_display_enable ? Event::Type::SetDisplayEnable : Event::Type::ResetDisplayEnable);
		}
	}
}

void Video::latch_word() {
	data_latch_[data_latch_position_] = ram_[current_address_ & 262143];
	++current_address_;
	++data_latch_position_;
	if(data_latch_position_ == 4) {
		data_latch_position_ = 0;
		shifter_.load(
			(uint64_t(data_latch_[0]) << 48) |
			(uint64_t(data_latch_[1]) << 32) |
			(uint64_t(data_latch_[2]) << 16) |
			uint64_t(data_latch_[3])
		);
	}
}

bool Video::hsync() {
	return horizontal_.sync;
}

bool Video::vsync() {
	return vertical_.sync;
}

bool Video::display_enabled() {
	return horizontal_.enable && vertical_.enable;
}

HalfCycles Video::get_next_sequence_point() {
	// The next sequence point will be whenever display_enabled, vsync or hsync next changes.

	// Sequence of events within a standard line:
	//
	//	1) blank disabled;
	//	2) display enabled;
	//	3) display disabled;
	//	4) blank enabled;
	//	5) sync enabled;
	//	6) sync disabled;
	//	7) end-of-line, potential vertical event.
	//
	// If this line has a vertical sync event on it, there will also be an event at cycle 30,
	// which will always falls somewhere between (1) and (4) but might or might not be in the
	// visible area.

	const auto horizontal_timings = horizontal_parameters(field_frequency_);

	// If this is a vertically-enabled line, check for the display enable boundaries.
	if(vertical_.enable) {
		/*
			TODO: what if there's a sync event scheduled for this line? That can happen with the
			lower border open.
		*/
		if(x_ < horizontal_timings.set_enable)		return HalfCycles(horizontal_timings.set_enable - x_);
		if(x_ < horizontal_timings.reset_enable) 	return HalfCycles(horizontal_timings.reset_enable - x_);
	} else {
		if(vertical_.sync_schedule != VerticalState::SyncSchedule::None && (x_ < 30*2)) {
			return HalfCycles(30*2 - x_);
		}
	}

	// Test for beginning and end of sync.
	if(x_ < line_length_ - 50) 	return HalfCycles(line_length_ - 50 - x_);
	if(x_ < line_length_ - 10) 	return HalfCycles(line_length_ - 10 - x_);

	// It wasn't any of those, so as a temporary expedient, just supply end of line.
	return HalfCycles(line_length_ - x_);
}

// MARK: - IO dispatch

uint16_t Video::read(int address) {
	address &= 0x3f;
	switch(address) {
		default:
		break;
		case 0x00:	return uint16_t(0xff00 | (base_address_ >> 16));
		case 0x01:	return uint16_t(0xff00 | (base_address_ >> 8));
		case 0x02:	return uint16_t(0xff00 | (current_address_ >> 15));	// Current address is kept in word precision internally;
		case 0x03:	return uint16_t(0xff00 | (current_address_ >> 7));	// the shifts here represent a conversion back to
		case 0x04:	return uint16_t(0xff00 | (current_address_ << 1));	// byte precision.

		case 0x05:	return sync_mode_ | 0xfcff;
		case 0x30:	return video_mode_ | 0xfcff;

		case 0x20:	case 0x21:	case 0x22:	case 0x23:
		case 0x24:	case 0x25:	case 0x26:	case 0x27:
		case 0x28:	case 0x29:	case 0x2a:	case 0x2b:
		case 0x2c:	case 0x2d:	case 0x2e:	case 0x2f: return raw_palette_[address - 0x20];
	}
	return 0xff;
}

void Video::write(int address, uint16_t value) {
	address &= 0x3f;
	switch(address) {
		default: break;

		// Start address.
		case 0x00:	base_address_ = (base_address_ & 0x00ffff) | ((value & 0xff) << 16);	break;
		case 0x01:	base_address_ = (base_address_ & 0xff00ff) | ((value & 0xff) << 8);		break;

		// Sync mode and pixel mode.
		case 0x05:
			sync_mode_ = value;
			update_output_mode();
		break;
		case 0x30:
			video_mode_ = value;
			update_output_mode();
		break;

		// Palette.
		case 0x20:	case 0x21:	case 0x22:	case 0x23:
		case 0x24:	case 0x25:	case 0x26:	case 0x27:
		case 0x28:	case 0x29:	case 0x2a:	case 0x2b:
		case 0x2c:	case 0x2d:	case 0x2e:	case 0x2f: {
			raw_palette_[address - 0x20] = value;
			uint8_t *const entry = reinterpret_cast<uint8_t *>(&palette_[address - 0x20]);
			entry[0] = uint8_t((value & 0x700) >> 7);
			entry[1] = uint8_t((value & 0x77) << 1);
		} break;
	}
}

void Video::update_output_mode() {
	// If this is black and white mode, that's that.
	switch((video_mode_ >> 8) & 3) {
		default:
		case 0:	output_bpp_ = OutputBpp::Four;	break;
		case 1:	output_bpp_ = OutputBpp::Two;	break;

		// 1bpp mode ignores the otherwise-programmed frequency.
		case 2:
			output_bpp_ = OutputBpp::One;
			field_frequency_ = FieldFrequency::SeventyTwo;
		return;
	}

	field_frequency_ = (sync_mode_ & 0x200) ? FieldFrequency::Fifty : FieldFrequency::Sixty;
}

// MARK: - The shifter

void Video::Shifter::flush_output(OutputMode next_mode) {
	switch(output_mode_) {
		case OutputMode::Sync:	crt_.output_sync(duration_);	break;
		case OutputMode::Blank:	crt_.output_blank(duration_);	break;
		case OutputMode::Border: {
//			if(!border_colour_) {
//				crt_.output_blank(duration_);
//			} else {
				uint16_t *const colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
				if(colour_pointer) *colour_pointer = border_colour_;
				crt_.output_level(duration_);
//			}
		} break;
		case OutputMode::Pixels: {
			crt_.output_data(duration_, pixel_pointer_);
			pixel_buffer_ = nullptr;
			pixel_pointer_ = 0;
		} break;
	}
	duration_ = 0;
	output_mode_ = next_mode;
}


void Video::Shifter::output_blank(int duration) {
	if(output_mode_ != OutputMode::Blank) {
		flush_output(OutputMode::Blank);
	}
	duration_ += duration;
}

void Video::Shifter::output_sync(int duration) {
	if(output_mode_ != OutputMode::Sync) {
		flush_output(OutputMode::Sync);
	}
	duration_ += duration;
}

void Video::Shifter::output_border(int duration, OutputBpp bpp) {
	// If there's still anything in the shifter, redirect this to an output_pixels call.
	if(output_shifter_) {
		// This doesn't take an opinion on how much of the shifter remains populated;
		// it assumes the worst case.
		const int pixel_length = std::min(32, duration);
		output_pixels(pixel_length, bpp);
		duration -= pixel_length;
		if(!duration) {
			return;
		}
	}

	// Flush anything that isn't level output *in the current border colour*.
	if(output_mode_ != OutputMode::Border || border_colour_ != palette_[0]) {
		flush_output(OutputMode::Border);
		border_colour_ = palette_[0];
	}
	duration_ += duration;
}

void Video::Shifter::output_pixels(int duration, OutputBpp bpp) {
	// If the shifter is empty and there's no pixel buffer at present,
	// redirect this to an output_level call. Otherwise, do a quick
	// memset-type fill, since the special case has been detected anyway.
	if(!output_shifter_) {
		if(!pixel_buffer_) {
			output_border(duration, bpp);
		} else {
			duration_ += duration;

			switch(bpp_) {
				case OutputBpp::One: {
					const size_t pixels = size_t(duration << 1);
					memset(&pixel_buffer_[pixel_pointer_], 0, pixels * sizeof(uint16_t));
					pixel_pointer_ += pixels;
				} break;

				default:
				case OutputBpp::Four:
					assert(!(duration & 1));
					duration >>= 1;
				case OutputBpp::Two: {
					while(duration--) {
						pixel_buffer_[pixel_pointer_] = palette_[0];
						++pixel_pointer_;
					}
				} break;
			}
		}
		return;
	}

	// Flush anything that isn't pixel output in the proper bpp; also flush if there's nowhere
	// left to put pixels.
	if(output_mode_ != OutputMode::Pixels || bpp_ != bpp || pixel_pointer_ >= 320) {
		flush_output(OutputMode::Pixels);
		bpp_ = bpp;
		pixel_buffer_ = reinterpret_cast<uint16_t *>(crt_.begin_data(320 + 32));
	}
	duration_ += duration;

	switch(bpp_) {
		case OutputBpp::One: {
			int pixels = duration << 1;
			if(pixel_buffer_) {
				while(pixels--) {
					pixel_buffer_[pixel_pointer_] = ((output_shifter_ >> 63) & 1) * 0xffff;
					output_shifter_ <<= 1;
					++pixel_pointer_;
				}
			} else {
				pixel_pointer_ += size_t(pixels);
				output_shifter_ <<= pixels;
			}
		} break;
		case OutputBpp::Two: {
	#if TARGET_RT_BIG_ENDIAN
			const int upper = 0;
	#else
			const int upper = 1;
	#endif
			if(pixel_buffer_) {
				while(duration--) {
					pixel_buffer_[pixel_pointer_] = palette_[
						((output_shifter_ >> 63) & 1) |
						((output_shifter_ >> 46) & 2)
					];
					// This ensures that the top two words shift one to the left;
					// their least significant bits are fed from the most significant bits
					// of the bottom two words, respectively.
					shifter_halves_[upper] = (shifter_halves_[upper] << 1) & 0xfffefffe;
					shifter_halves_[upper] |= (shifter_halves_[upper^1] & 0x80008000) >> 15;
					shifter_halves_[upper^1] = (shifter_halves_[upper^1] << 1) & 0xfffefffe;

					++pixel_pointer_;
				}
			} else {
				pixel_pointer_ += size_t(duration);
				while(duration--) {
					shifter_halves_[upper] = (shifter_halves_[upper] << 1) & 0xfffefffe;
					shifter_halves_[upper] |= (shifter_halves_[upper^1] & 0x80008000) >> 15;
					shifter_halves_[upper^1] = (shifter_halves_[upper^1] << 1) & 0xfffefffe;
				}
			}
		} break;
		default:
		case OutputBpp::Four:
			assert(!(duration & 1));
			if(pixel_buffer_) {
				while(duration) {
					pixel_buffer_[pixel_pointer_] = palette_[
						((output_shifter_ >> 63) & 1) |
						((output_shifter_ >> 46) & 2) |
						((output_shifter_ >> 29) & 4) |
						((output_shifter_ >> 12) & 8)
					];
					output_shifter_ = (output_shifter_ << 1) & 0xfffefffefffefffe;
					++pixel_pointer_;
					duration -= 2;
				}
			} else {
				pixel_pointer_ += size_t(duration >> 1);
				while(duration) {
					output_shifter_ = (output_shifter_ << 1) & 0xfffefffefffefffe;
					duration -= 2;
				}
			}
		break;
	}
}

void Video::Shifter::load(uint64_t value) {
	output_shifter_ = value;
}
