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

#define CYCLE(x)	((x) * 2)

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
	{34, 434, 500}	// Guesswork: (i) nobody ever recommends 72Hz mode for opening the top border, so it's likely to be the same as another mode; (ii) being the same as PAL feels too late.
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

	const int vertical_decision;

	LineLength length;
} horizontal_params[3] = {
	{CYCLE(56), CYCLE(376),		CYCLE(450), CYCLE(28),		CYCLE(502),		{ CYCLE(512), CYCLE(464), CYCLE(504) }},
	{CYCLE(52), CYCLE(372),		CYCLE(450), CYCLE(24),		CYCLE(502),		{ CYCLE(508), CYCLE(460), CYCLE(500) }},
	{CYCLE(4),	CYCLE(164),		CYCLE(999), CYCLE(999),		CYCLE(214),		{ CYCLE(224), CYCLE(194), CYCLE(212) }}
		// 72Hz mode doesn't set or reset blank.
};

// Re: 'vertical_decision':
// This is cycle 502 if in 50 or 60 Hz mode; in 70 Hz mode I've put it on cycle 214
// in order to be analogous to 50 and 60 Hz mode. I have no idea where it should
// actually go.
//
// Ditto the horizontal sync timings for 72Hz are plucked out of thin air.

const HorizontalParams &horizontal_parameters(Video::FieldFrequency frequency) {
	return horizontal_params[int(frequency)];
}

#ifndef NDEBUG
struct Checker {
	Checker() {
		for(int c = 0; c < 3; ++c) {
			// Expected horizontal order of events: reset blank, enable display, disable display, enable blank (at least 50 before end of line), end of line
			const auto horizontal = horizontal_parameters(Video::FieldFrequency(c));

			if(c < 2) {
				assert(horizontal.reset_blank < horizontal.set_enable);
				assert(horizontal.set_enable < horizontal.reset_enable);
				assert(horizontal.reset_enable < horizontal.set_blank);
				assert(horizontal.set_blank+50 < horizontal.length.length);
			} else {
				assert(horizontal.set_enable < horizontal.reset_enable);
				assert(horizontal.set_enable+50 <horizontal.length.length);
			}

			// Expected vertical order of events: reset blank, enable display, disable display, enable blank (at least 50 before end of line), end of line
			const auto vertical = vertical_parameters(Video::FieldFrequency(c));
			assert(vertical.set_enable < vertical.reset_enable);
			assert(vertical.reset_enable < vertical.height);
		}
	}
} checker;
#endif

const int de_delay_period = CYCLE(28);		// Amount of time after DE that observed DE changes. NB: HACK HERE. This currently incorporates the MFP recognition delay. MUST FIX.
const int vsync_x_position = CYCLE(56);		// Horizontal cycle on which vertical sync changes happen.

const int line_length_latch_position = CYCLE(54);

const int hsync_delay_period = CYCLE(8);			// Signal hsync at the end of the line.
const int vsync_delay_period = hsync_delay_period;	// Signal vsync with the same delay as hsync.

const int load_delay_period = CYCLE(4);		// Amount of time after DE that observed DE changes. NB: HACK HERE. This currently incorporates the MFP recognition delay. MUST FIX.

// "VSYNC starts 104 cycles after the start of the previous line's HSYNC, so that's 4 cycles before DE would be activated. ";
// that's an inconsistent statement since it would imply VSYNC at +54, which is 2 cycles before DE in 60Hz mode and 6 before
// in 50Hz mode. I've gone with 56, to be four cycles ahead of DE in 50Hz mode.

}

Video::Video() :
	crt_(2048, 2, Outputs::Display::Type::PAL50, Outputs::Display::InputDataType::Red4Green4Blue4),
//	crt_(896, 1, 500, 5, Outputs::Display::InputDataType::Red4Green4Blue4),
	video_stream_(crt_, palette_) {

	// Show a total of 260 lines; a little short for PAL but a compromise between that and the ST's
	// usual output height of 200 lines.
	crt_.set_visible_area(crt_.get_rect_for_area(33, 260, 440, 1700, 4.0f / 3.0f));
}

void Video::set_ram(uint16_t *ram, size_t size) {
	ram_ = ram;
	ram_mask_ = int((size >> 1) - 1);
}

void Video::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	crt_.set_scan_target(scan_target);
}

Outputs::Display::ScanStatus Video::get_scaled_scan_status() const {
	return crt_.get_scaled_scan_status() / 4.0f;
}

void Video::set_display_type(Outputs::Display::DisplayType display_type) {
	crt_.set_display_type(display_type);
}

Outputs::Display::DisplayType Video::get_display_type() const {
	return crt_.get_display_type();
}

void Video::run_for(HalfCycles duration) {
	int integer_duration = int(duration.as_integral());
	assert(integer_duration >= 0);

	while(integer_duration) {
		const auto horizontal_timings = horizontal_parameters(field_frequency_);
		const auto vertical_timings = vertical_parameters(field_frequency_);

		// Determine time to next event; this'll either be one of the ones informally scheduled in here,
		// or something from the deferral queue.

		// Seed next event to end of line.
		int next_event = line_length_.length;

		const int next_deferred_event = deferrer_.time_until_next_action().as<int>();
		if(next_deferred_event >= 0)
			next_event = std::min(next_event, next_deferred_event + x_);

		// Check the explicitly-placed events.
		if(horizontal_timings.reset_blank > x_)		next_event = std::min(next_event, horizontal_timings.reset_blank);
		if(horizontal_timings.set_blank > x_)		next_event = std::min(next_event, horizontal_timings.set_blank);
		if(horizontal_timings.reset_enable > x_)	next_event = std::min(next_event, horizontal_timings.reset_enable);
		if(horizontal_timings.set_enable > x_) 		next_event = std::min(next_event, horizontal_timings.set_enable);

		// Check for events that are relative to existing latched state.
		if(line_length_.hsync_start > x_)			next_event = std::min(next_event, line_length_.hsync_start);
		if(line_length_.hsync_end > x_)				next_event = std::min(next_event, line_length_.hsync_end);

		// Also, a vertical sync event might intercede.
		if(vertical_.sync_schedule != VerticalState::SyncSchedule::None && x_ < vsync_x_position && next_event >= vsync_x_position) {
			next_event = vsync_x_position;
		}

		// Determine current output mode and number of cycles to output for.
		const int run_length = std::min(integer_duration, next_event - x_);
		const bool display_enable = vertical_.enable && horizontal_.enable;
		const bool hsync = horizontal_.sync;
		const bool vsync = vertical_.sync;

		assert(run_length > 0);

		// Ensure proper fetching irrespective of the output.
		if(load_) {
			const int since_load = x_ - load_base_;

			// There will be pixels this line, subject to the shifter pipeline.
			// Divide into 8-[half-]cycle windows; at the start of each window fetch a word,
			// and during the rest of the window, shift out.
			int start_column = (since_load - 1) >> 3;
			const int end_column = (since_load + run_length - 1) >> 3;

			while(start_column != end_column) {
				data_latch_[data_latch_position_] = ram_[current_address_ & ram_mask_];
				data_latch_position_ = (data_latch_position_ + 1) & 127;
				++current_address_;
				++start_column;
			}
		}

		if(horizontal_.sync || vertical_.sync) {
			video_stream_.output(run_length, VideoStream::OutputMode::Sync);
		} else if(horizontal_.blank || vertical_.blank) {
			video_stream_.output(run_length, VideoStream::OutputMode::Blank);
		} else if(!load_) {
			video_stream_.output(run_length, VideoStream::OutputMode::Pixels);
		} else {
			const int start = x_ - load_base_;
			const int end = start + run_length;

			// There will be pixels this line, subject to the shifter pipeline.
			// Divide into 8-[half-]cycle windows; at the start of each window fetch a word,
			// and during the rest of the window, shift out.
			int start_column = start >> 3;
			const int end_column = end >> 3;
			const int start_offset = start & 7;
			const int end_offset = end & 7;

			// Rules obeyed below:
			//
			//	Video fetches occur as the first act of business in a column. Each
			//	fetch is then followed by 8 shift clocks. Whether or not the shifter
			//	was reloaded by the fetch depends on the FIFO.

			if(start_column == end_column) {
				if(!start_offset) {
					push_latched_data();
				}
				video_stream_.output(run_length, VideoStream::OutputMode::Pixels);
			} else {
				// Continue the current column if partway across.
				if(start_offset) {
					// If at least one column boundary is crossed, complete this column.
					video_stream_.output(8 - start_offset, VideoStream::OutputMode::Pixels);
					++start_column;	// This starts a new column, so latch a new word.
				}

				// Run for all columns that have their starts in this time period.
				int complete_columns = end_column - start_column;
				while(complete_columns--) {
					push_latched_data();
					video_stream_.output(8, VideoStream::OutputMode::Pixels);
				}

				// Output the start of the next column, if necessary.
				if(end_offset) {
					push_latched_data();
					video_stream_.output(end_offset, VideoStream::OutputMode::Pixels);
				}
			}
		}

		// Check for whether line length should have been latched during this run.
		if(x_ < line_length_latch_position && (x_ + run_length) >= line_length_latch_position) {
			line_length_ = horizontal_timings.length;
		}

		// Make a decision about vertical state on the appropriate cycle.
		if(x_ < horizontal_timings.vertical_decision && (x_ + run_length) >= horizontal_timings.vertical_decision) {
			next_y_ = y_ + 1;
			next_vertical_ = vertical_;
			next_vertical_.sync_schedule = VerticalState::SyncSchedule::None;

			// Use vertical_parameters to get parameters for the current output frequency;
			// quick note: things other than the total frame size are counted in terms
			// of the line they're evaluated on — i.e. the test is this line, not the next
			// one. The total height constraint is obviously whether the next one would be
			// too far.
			if(y_ == vertical_timings.set_enable) {
				next_vertical_.enable = true;
			} else if(y_ == vertical_timings.reset_enable) {
				next_vertical_.enable = false;
			} else if(next_y_ == vertical_timings.height - 2) {
				next_vertical_.sync_schedule = VerticalState::SyncSchedule::Begin;
			} else if(next_y_ == vertical_timings.height) {
				next_y_ = 0;
			} else if(y_ == 0) {
				next_vertical_.sync_schedule = VerticalState::SyncSchedule::End;
			}
		}

		// Apply the next event.
		x_ += run_length;
		assert(integer_duration >= run_length);
		integer_duration -= run_length;
		deferrer_.advance(HalfCycles(run_length));

		// Check horizontal events; the first six are guaranteed to occur separately.
		if(horizontal_timings.reset_blank == x_)		horizontal_.blank = false;
		else if(horizontal_timings.set_blank == x_)		horizontal_.blank = true;
		else if(horizontal_timings.reset_enable == x_)	horizontal_.enable = false;
		else if(horizontal_timings.set_enable == x_) 	horizontal_.enable = true;
		else if(line_length_.hsync_start == x_)			{ horizontal_.sync = true; horizontal_.enable = false; }
		else if(line_length_.hsync_end == x_)			horizontal_.sync = false;

		// Check vertical events.
		if(vertical_.sync_schedule != VerticalState::SyncSchedule::None && x_ == vsync_x_position) {
			vertical_.sync = vertical_.sync_schedule == VerticalState::SyncSchedule::Begin;
			vertical_.enable &= !vertical_.sync;

			reset_fifo();	// TODO: remove this, probably, once otherwise stable?
		}

		// Check whether the terminating event was end-of-line; if so then advance
		// the vertical bits of state.
		if(x_ == line_length_.length) {
			x_ = 0;
			vertical_ = next_vertical_;
			y_ = next_y_;
		}

		// The address is reloaded during the entire period of vertical sync.
		// Cf. http://www.atari-forum.com/viewtopic.php?t=31954&start=50#p324730
		if(vertical_.sync) {
			current_address_ = base_address_ >> 1;

			// Consider a shout out to the range observer.
			if(previous_base_address_ != base_address_) {
				previous_base_address_ = base_address_;
				if(range_observer_) {
					range_observer_->video_did_change_access_range(this);
				}
			}
		}

		// Chuck any deferred output changes into the queue.
		const bool next_display_enable = vertical_.enable && horizontal_.enable;
		if(display_enable != next_display_enable) {
			// Schedule change in load line.
			deferrer_.defer(load_delay_period, [this, next_display_enable] {
				this->load_ = next_display_enable;
				this->load_base_ = this->x_;
			});

			// Schedule change in outwardly-visible DE line.
			deferrer_.defer(de_delay_period, [this, next_display_enable] {
				this->public_state_.display_enable = next_display_enable;
			});
		}

		if(horizontal_.sync != hsync) {
			// Schedule change in outwardly-visible hsync line.
			deferrer_.defer(hsync_delay_period, [this, next_horizontal_sync = horizontal_.sync] {
				this->public_state_.hsync = next_horizontal_sync;
			});
		}

		if(vertical_.sync != vsync) {
			// Schedule change in outwardly-visible hsync line.
			deferrer_.defer(vsync_delay_period, [this, next_vertical_sync = vertical_.sync] {
				this->public_state_.vsync = next_vertical_sync;
			});
		}
	}
}

void Video::push_latched_data() {
	data_latch_read_position_ = (data_latch_read_position_ + 1) & 127;

	if(!(data_latch_read_position_ & 3)) {
		video_stream_.load(
			(uint64_t(data_latch_[(data_latch_read_position_ - 4) & 127]) << 48) |
			(uint64_t(data_latch_[(data_latch_read_position_ - 3) & 127]) << 32) |
			(uint64_t(data_latch_[(data_latch_read_position_ - 2) & 127]) << 16) |
			uint64_t(data_latch_[(data_latch_read_position_ - 1) & 127])
		);
	}
}

void Video::reset_fifo() {
	data_latch_read_position_ = data_latch_position_ = 0;
}

bool Video::hsync() {
	return public_state_.hsync;
}

bool Video::vsync() {
	return public_state_.vsync;
}

bool Video::display_enabled() {
	return public_state_.display_enable;
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

	int event_time = line_length_.length;	// Worst case: report end of line.

	// If any events are pending, give the first of those the chance to be next.
	const auto next_deferred_item = deferrer_.time_until_next_action();
	if(next_deferred_item != HalfCycles(-1)) {
		event_time = std::min(event_time, x_ + next_deferred_item.as<int>());
	}

	// If this is a vertically-enabled line, check for the display enable boundaries, + the standard delay.
	if(vertical_.enable) {
		if(x_ < horizontal_timings.set_enable + de_delay_period) {
			event_time = std::min(event_time, horizontal_timings.set_enable + de_delay_period);
		}
		else if(x_ < horizontal_timings.reset_enable + de_delay_period) {
			event_time = std::min(event_time, horizontal_timings.reset_enable + de_delay_period);
		}
	}

	// If a vertical sync event is scheduled, test for that.
	if(vertical_.sync_schedule != VerticalState::SyncSchedule::None && (x_ < vsync_x_position)) {
		event_time = std::min(event_time, vsync_x_position);
	}

	// Test for beginning and end of horizontal sync.
	if(x_ < line_length_.hsync_start + hsync_delay_period) {
		event_time = std::min(line_length_.hsync_start + hsync_delay_period, event_time);
	}
	if(x_ < line_length_.hsync_end + hsync_delay_period) {
		event_time = std::min(line_length_.hsync_end + hsync_delay_period, event_time);
	}

	// Also factor in the line length latching time.
	if(x_ < line_length_latch_position) {
		event_time = std::min(line_length_latch_position, event_time);
	}

	// It wasn't any of those, just supply end of line. That's when the static_assert above assumes a visible hsync transition.
	return HalfCycles(event_time - x_);
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
			// Writes to sync mode have a one-cycle delay in effect.
			deferrer_.defer(HalfCycles(2), [this, value] {
				sync_mode_ = value;
				update_output_mode();
			});
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
			if(address == 0x20) video_stream_.will_change_border_colour();

			raw_palette_[address - 0x20] = value;
			uint8_t *const entry = reinterpret_cast<uint8_t *>(&palette_[address - 0x20]);
			entry[0] = uint8_t((value & 0x700) >> 7);
			entry[1] = uint8_t((value & 0x77) << 1);
		} break;
	}
}

void Video::update_output_mode() {
	const auto old_bpp_ = output_bpp_;

	// If this is black and white mode, that's that.
	switch((video_mode_ >> 8) & 3) {
		case 0:	output_bpp_ = OutputBpp::Four;	break;
		case 1:	output_bpp_ = OutputBpp::Two;	break;
		default:
		case 2:	output_bpp_ = OutputBpp::One;	break;
	}

	// 1bpp mode ignores the otherwise-programmed frequency.
	if(output_bpp_ == OutputBpp::One) {
		field_frequency_ = FieldFrequency::SeventyTwo;
	} else {
		field_frequency_ = (sync_mode_ & 0x200) ? FieldFrequency::Fifty : FieldFrequency::Sixty;
	}
	if(output_bpp_ != old_bpp_) {
		// "the 71-Hz-switch does something like a shifter-reset." (and some people use a high-low resolutions switch instead)
		reset_fifo();
		video_stream_.set_bpp(output_bpp_);
	}

//	const int freqs[] = {50, 60, 72};
//	printf("%d, %d -> %d [%d %d]\n", x_ / 2, y_, freqs[int(field_frequency_)], horizontal_.enable, vertical_.enable);
}

// MARK: - The shifter

void Video::VideoStream::output(int duration, OutputMode mode) {
	// If this is a transition from sync to blank, actually transition to colour burst.
	if(output_mode_ == OutputMode::Sync && mode == OutputMode::Blank) {
		mode = OutputMode::ColourBurst;
	}

	// If this is seeming a transition from blank to colour burst, obey it only if/when
	// sufficient colour burst has been output.
	if(output_mode_ == OutputMode::Blank && mode == OutputMode::ColourBurst) {
		if(duration_ + duration >= 40) {
			const int overage = duration + duration_ - 40;
			duration_ = 40;

			generate(overage, OutputMode::ColourBurst, true);
		} else {
			mode = OutputMode::ColourBurst;
		}
	}

	// If this is a transition, or if we're doing pixels, output whatever has been accumulated.
	if(mode != output_mode_ || output_mode_ == OutputMode::Pixels) {
		generate(duration, output_mode_, mode != output_mode_);
	} else {
		duration_ += duration;
	}

	// Accumulate time in the current mode.
	output_mode_ = mode;
}

void Video::VideoStream::generate(int duration, OutputMode mode, bool is_terminal) {
	// Three of these are trivial; deal with them upfront. They don't care about the duration of
	// whatever is new, just about how much was accumulated prior to now.
	if(mode != OutputMode::Pixels) {
		switch(mode) {
			default:
			case OutputMode::Sync:			crt_.output_sync(duration_*2);					break;
			case OutputMode::Blank:			crt_.output_blank(duration_*2);					break;
			case OutputMode::ColourBurst:	crt_.output_default_colour_burst(duration_*2);	break;
		}

		// Reseed duration.
		duration_ = duration;

		// The shifter should keep running, so throw away the proper amount of content.
		shift(duration_);

		return;
	}

	// If the shifter is empty, accumulate in duration_ a promise to draw border later.
	if(!output_shifter_) {
		if(pixel_pointer_) {
			flush_pixels();
		}

		duration_ += duration;

		// If this is terminal, we'll need to draw now. But if it isn't, job done.
		if(is_terminal) {
			flush_border();
		}

		return;
	}

	// There's definitely some pixels to convey, but perhaps there's some border first?
	if(duration_) {
		flush_border();
	}

	// Time to do some pixels!
	output_pixels(duration);

	// If was terminal, make sure any transient storage is output.
	if(is_terminal) {
		flush_pixels();
	}
}

void Video::VideoStream::will_change_border_colour() {
	// Flush the accumulated border if it'd be adversely affected.
	if(duration_ && output_mode_ == OutputMode::Pixels) {
		flush_border();
	}
}

void Video::VideoStream::flush_border() {
	// Output colour 0 for the entirety of duration_ (or black, if this is 1bpp mode).
	uint16_t *const colour_pointer = reinterpret_cast<uint16_t *>(crt_.begin_data(1));
	if(colour_pointer) *colour_pointer = (bpp_ != OutputBpp::One) ?  palette_[0] : 0;
	crt_.output_level(duration_*2);

	duration_ = 0;
}

namespace {
#if TARGET_RT_BIG_ENDIAN
	constexpr int upper = 0;
#else
	constexpr int upper = 1;
#endif
}

void Video::VideoStream::shift(int duration) {
	switch(bpp_) {
		case OutputBpp::One:
			output_shifter_ <<= (duration << 1);
		break;
		case OutputBpp::Two:
			while(duration--) {
				shifter_halves_[upper] = (shifter_halves_[upper] << 1) & 0xfffefffe;
				shifter_halves_[upper] |= (shifter_halves_[upper^1] & 0x80008000) >> 15;
				shifter_halves_[upper^1] = (shifter_halves_[upper^1] << 1) & 0xfffefffe;
			}
		break;
		case OutputBpp::Four:
			while(duration) {
				output_shifter_ = (output_shifter_ << 1) & 0xfffefffefffefffe;
				duration -= 2;
			}
		break;
	}
}

// TODO: turn this into a template on current BPP, perhaps? Would avoid reevaluation of the conditional.
void Video::VideoStream::output_pixels(int duration) {
	constexpr int allocation_size = 352;	// i.e. 320 plus a spare 32.

	// Convert from duration to pixels.
	int pixels = duration;
	switch(bpp_) {
		case OutputBpp::One: pixels <<= 1;	break;
		default: break;
		case OutputBpp::Four: pixels >>= 1;	break;
	}

	while(pixels) {
		// If no buffer is currently available, attempt to allocate one.
		if(!pixel_buffer_) {
			pixel_buffer_ = reinterpret_cast<uint16_t *>(crt_.begin_data(allocation_size, 2));

			// Stop the loop if no buffer is available.
			if(!pixel_buffer_) break;
		}

		int pixels_to_draw = std::min(allocation_size - pixel_pointer_, pixels);
		pixels -= pixels_to_draw;

		switch(bpp_) {
			case OutputBpp::One:
				while(pixels_to_draw--) {
					pixel_buffer_[pixel_pointer_] = ((output_shifter_ >> 63) & 1) * 0xffff;
					output_shifter_ <<= 1;

					++pixel_pointer_;
				}
			break;

			case OutputBpp::Two:
				while(pixels_to_draw--) {
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
			break;

			case OutputBpp::Four:
				while(pixels_to_draw--) {
					pixel_buffer_[pixel_pointer_] = palette_[
						((output_shifter_ >> 63) & 1) |
						((output_shifter_ >> 46) & 2) |
						((output_shifter_ >> 29) & 4) |
						((output_shifter_ >> 12) & 8)
					];
					output_shifter_ = (output_shifter_ << 1) & 0xfffefffefffefffe;

					++pixel_pointer_;
				}
			break;
		}

		// Check whether the limit has been reached.
		if(pixel_pointer_ >= allocation_size - 32) {
			flush_pixels();
		}
	}

	// If duration remains, that implies no buffer was available, so
	// just do the corresponding shifting and provide proper timing to the CRT.
	if(pixels) {
		int leftover_duration = pixels;
		switch(bpp_) {
			default: 				leftover_duration >>= 1;	break;
			case OutputBpp::Two:								break;
			case OutputBpp::Four:	leftover_duration <<= 1;	break;
		}
		shift(leftover_duration);
		crt_.output_data(leftover_duration*2);
	}
}

void Video::VideoStream::flush_pixels() {
	// Flush only if there's something to flush.
	if(pixel_pointer_) {
		switch(bpp_) {
			case OutputBpp::One:	crt_.output_data(pixel_pointer_); 								break;
			default:				crt_.output_data(pixel_pointer_ << 1, size_t(pixel_pointer_));	break;
			case OutputBpp::Four:	crt_.output_data(pixel_pointer_ << 2, size_t(pixel_pointer_));	break;
		}
	}

	pixel_pointer_ = 0;
	pixel_buffer_ = nullptr;
}

void Video::VideoStream::set_bpp(OutputBpp bpp) {
	// Terminate the allocated block of memory (if any).
	flush_pixels();

	// Reset the shifter.
	// TODO: is flushing like this correct?
	output_shifter_ = 0;

	// Store the new BPP.
	bpp_ = bpp;
}

void Video::VideoStream::load(uint64_t value) {
	// In 1bpp mode, a 0 bit is white and a 1 bit is black.
	// Invert the input so that the 'just output the border colour
	// when the shifter is empty' optimisation works.
	if(bpp_ == OutputBpp::One)
		output_shifter_ = ~value;
	else
		output_shifter_ = value;
}

// MARK: - Range observer.

Video::Range Video::get_memory_access_range() {
	Range range;
	range.low_address = uint32_t(previous_base_address_);
	range.high_address = range.low_address + 56994;
	// 56994 is pessimistic but unscientific, being derived from the resolution of the largest
	// fullscreen demo I could quickly find documentation of. TODO: calculate real number.
	return range;
}

void Video::set_range_observer(RangeObserver *observer) {
	range_observer_ = observer;
	observer->video_did_change_access_range(this);
}
