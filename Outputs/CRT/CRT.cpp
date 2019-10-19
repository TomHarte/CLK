//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"

#include <cstdarg>
#include <cmath>
#include <algorithm>
#include <cassert>

using namespace Outputs::CRT;

void CRT::set_new_timing(int cycles_per_line, int height_of_display, Outputs::Display::ColourSpace colour_space, int colour_cycle_numerator, int colour_cycle_denominator, int vertical_sync_half_lines, bool should_alternate) {

	const int millisecondsHorizontalRetraceTime = 7;	// Source: Dictionary of Video and Television Technology, p. 234.
	const int scanlinesVerticalRetraceTime = 8;			// Source: ibid.

														// To quote:
														//
														//	"retrace interval; The interval of time for the return of the blanked scanning beam of
														//	a TV picture tube or camera tube to the starting point of a line or field. It is about
														//	7 microseconds for horizontal retrace and 500 to 750 microseconds for vertical retrace
														//  in NTSC and PAL TV."

	time_multiplier_ = 65535 / cycles_per_line;
	phase_denominator_ = int64_t(cycles_per_line) * int64_t(colour_cycle_denominator) * int64_t(time_multiplier_);
	phase_numerator_ = 0;
	colour_cycle_numerator_ = int64_t(colour_cycle_numerator);
	phase_alternates_ = should_alternate;
	is_alernate_line_ &= phase_alternates_;
	cycles_per_line_ = cycles_per_line;
	const int multiplied_cycles_per_line = cycles_per_line * time_multiplier_;

	// Allow sync to be detected (and acted upon) a line earlier than the specified requirement,
	// as a simple way of avoiding not-quite-exact comparison issues while still being true enough to
	// the gist for simple debugging.
	sync_capacitor_charge_threshold_ = ((vertical_sync_half_lines - 2) * cycles_per_line) >> 1;

	// Create the two flywheels:
	//
	// The horizontal flywheel has an ideal period of `multiplied_cycles_per_line`, will accept syncs
	// within 1/32nd of that (i.e. tolerates 3.125% error) and takes millisecondsHorizontalRetraceTime
	// to retrace.
	//
	// The vertical slywheel has an ideal period of `multiplied_cycles_per_line * height_of_display`,
	// will accept syncs within 1/8th of that (i.e. tolerates 12.5% error) and takes scanlinesVerticalRetraceTime
	// to retrace.
	horizontal_flywheel_.reset(new Flywheel(multiplied_cycles_per_line, (millisecondsHorizontalRetraceTime * multiplied_cycles_per_line) >> 6, multiplied_cycles_per_line >> 5));
	vertical_flywheel_.reset(new Flywheel(multiplied_cycles_per_line * height_of_display, scanlinesVerticalRetraceTime * multiplied_cycles_per_line, (multiplied_cycles_per_line * height_of_display) >> 3));

	// Figure out the divisor necessary to get the horizontal flywheel into a 16-bit range.
	const int real_clock_scan_period = vertical_flywheel_->get_scan_period();
	vertical_flywheel_output_divider_ = (real_clock_scan_period + 65534) / 65535;

	// Communicate relevant fields to the scan target.
	scan_target_modals_.output_scale.x = uint16_t(horizontal_flywheel_->get_scan_period());
	scan_target_modals_.output_scale.y = uint16_t(real_clock_scan_period / vertical_flywheel_output_divider_);
	scan_target_modals_.expected_vertical_lines = height_of_display;
	scan_target_modals_.composite_colour_space = colour_space;
	scan_target_modals_.colour_cycle_numerator = colour_cycle_numerator;
	scan_target_modals_.colour_cycle_denominator = colour_cycle_denominator;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_scan_target(Outputs::Display::ScanTarget *scan_target) {
	scan_target_ = scan_target;
	if(!scan_target_) scan_target_ = &Outputs::Display::NullScanTarget::singleton;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_new_data_type(Outputs::Display::InputDataType data_type) {
	scan_target_modals_.input_data_type = data_type;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_aspect_ratio(float aspect_ratio) {
	scan_target_modals_.aspect_ratio = aspect_ratio;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_visible_area(Outputs::Display::Rect visible_area) {
	scan_target_modals_.visible_area = visible_area;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_display_type(Outputs::Display::DisplayType display_type) {
	scan_target_modals_.display_type = display_type;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_phase_linked_luminance_offset(float offset) {
	scan_target_modals_.input_data_tweaks.phase_linked_luminance_offset = offset;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_input_data_type(Outputs::Display::InputDataType input_data_type) {
	scan_target_modals_.input_data_type = input_data_type;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_brightness(float brightness) {
	scan_target_modals_.brightness = brightness;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_new_display_type(int cycles_per_line, Outputs::Display::Type displayType) {
	switch(displayType) {
		case Outputs::Display::Type::PAL50:
			scan_target_modals_.intended_gamma = 2.8f;
			set_new_timing(cycles_per_line, 312, Outputs::Display::ColourSpace::YUV, 709379, 2500, 5, true);	// i.e. 283.7516 colour cycles per line; 2.5 lines = vertical sync.
		break;

		case Outputs::Display::Type::NTSC60:
			scan_target_modals_.intended_gamma = 2.2f;
			set_new_timing(cycles_per_line, 262, Outputs::Display::ColourSpace::YIQ, 455, 2, 6, false);			// i.e. 227.5 colour cycles per line, 3 lines = vertical sync.
		break;
	}
}

void CRT::set_composite_function_type(CompositeSourceType type, float offset_of_first_sample) {
	if(type == DiscreteFourSamplesPerCycle) {
		colour_burst_phase_adjustment_ = static_cast<uint8_t>(offset_of_first_sample * 256.0f) & 63;
	} else {
		colour_burst_phase_adjustment_ = 0xff;
	}
}

void CRT::set_input_gamma(float gamma) {
	scan_target_modals_.intended_gamma = gamma;
	scan_target_->set_modals(scan_target_modals_);
}

CRT::CRT(	int cycles_per_line,
			int clocks_per_pixel_greatest_common_divisor,
			int height_of_display,
			Outputs::Display::ColourSpace colour_space,
			int colour_cycle_numerator, int colour_cycle_denominator,
			int vertical_sync_half_lines,
			bool should_alternate,
			Outputs::Display::InputDataType data_type) {
	scan_target_modals_.input_data_type = data_type;
	scan_target_modals_.cycles_per_line = cycles_per_line;
	scan_target_modals_.clocks_per_pixel_greatest_common_divisor = clocks_per_pixel_greatest_common_divisor;
	set_new_timing(cycles_per_line, height_of_display, colour_space, colour_cycle_numerator, colour_cycle_denominator, vertical_sync_half_lines, should_alternate);
}

CRT::CRT(	int cycles_per_line,
			int clocks_per_pixel_greatest_common_divisor,
			Outputs::Display::Type display_type,
			Outputs::Display::InputDataType data_type) {
	scan_target_modals_.input_data_type = data_type;
	scan_target_modals_.cycles_per_line = cycles_per_line;
	scan_target_modals_.clocks_per_pixel_greatest_common_divisor = clocks_per_pixel_greatest_common_divisor;
	set_new_display_type(cycles_per_line, display_type);
}

// MARK: - Sync loop

Flywheel::SyncEvent CRT::get_next_vertical_sync_event(bool vsync_is_requested, int cycles_to_run_for, int *cycles_advanced) {
	return vertical_flywheel_->get_next_event_in_period(vsync_is_requested, cycles_to_run_for, cycles_advanced);
}

Flywheel::SyncEvent CRT::get_next_horizontal_sync_event(bool hsync_is_requested, int cycles_to_run_for, int *cycles_advanced) {
	return horizontal_flywheel_->get_next_event_in_period(hsync_is_requested, cycles_to_run_for, cycles_advanced);
}

Outputs::Display::ScanTarget::Scan::EndPoint CRT::end_point(uint16_t data_offset) {
	Display::ScanTarget::Scan::EndPoint end_point;

	end_point.x = uint16_t(horizontal_flywheel_->get_current_output_position());
	end_point.y = uint16_t(vertical_flywheel_->get_current_output_position() / vertical_flywheel_output_divider_);
	end_point.data_offset = data_offset;

	// TODO: this is a workaround for the limited precision that can be posted onwards;
	// it'd be better to make time_multiplier_ an explicit modal and just not divide by it.
	const auto lost_precision = cycles_since_horizontal_sync_ % time_multiplier_;
	end_point.composite_angle = int16_t(((phase_numerator_ - lost_precision * colour_cycle_numerator_) << 6) / phase_denominator_) * (is_alernate_line_ ? -1 : 1);
	end_point.cycles_since_end_of_horizontal_retrace = uint16_t(cycles_since_horizontal_sync_ / time_multiplier_);

	return end_point;
}

void CRT::advance_cycles(int number_of_cycles, bool hsync_requested, bool vsync_requested, const Scan::Type type, int number_of_samples) {
	number_of_cycles *= time_multiplier_;

	const bool is_output_run = ((type == Scan::Type::Level) || (type == Scan::Type::Data));
	const auto total_cycles = number_of_cycles;
	bool did_output = false;

	while(number_of_cycles) {

		// Get time until next horizontal and vertical sync generator events.
		int time_until_vertical_sync_event, time_until_horizontal_sync_event;
		const Flywheel::SyncEvent next_vertical_sync_event = get_next_vertical_sync_event(vsync_requested, number_of_cycles, &time_until_vertical_sync_event);
		const Flywheel::SyncEvent next_horizontal_sync_event = get_next_horizontal_sync_event(hsync_requested, time_until_vertical_sync_event, &time_until_horizontal_sync_event);

		// Whichever event is scheduled to happen first is the one to advance to.
		const int next_run_length = std::min(time_until_vertical_sync_event, time_until_horizontal_sync_event);

		hsync_requested = false;
		vsync_requested = false;

		// Determine whether to output any data for this portion of the output; if so then grab somewhere to put it.
		const bool is_output_segment = ((is_output_run && next_run_length) && !horizontal_flywheel_->is_in_retrace() && !vertical_flywheel_->is_in_retrace());
		Outputs::Display::ScanTarget::Scan *const next_scan = is_output_segment ? scan_target_->begin_scan() : nullptr;
		did_output |= is_output_segment;

		// If outputting, store the start location and scan constants.
		if(next_scan) {
			next_scan->end_points[0] = end_point(uint16_t((total_cycles - number_of_cycles) * number_of_samples / total_cycles));
			next_scan->composite_amplitude = colour_burst_amplitude_;
		}

		// Advance time: that'll affect both the colour subcarrier position and the number of cycles left to run.
		phase_numerator_ += next_run_length * colour_cycle_numerator_;
		number_of_cycles -= next_run_length;
		cycles_since_horizontal_sync_ += next_run_length;

		// React to the incoming event.
		horizontal_flywheel_->apply_event(next_run_length, (next_run_length == time_until_horizontal_sync_event) ? next_horizontal_sync_event : Flywheel::SyncEvent::None);
		vertical_flywheel_->apply_event(next_run_length, (next_run_length == time_until_vertical_sync_event) ? next_vertical_sync_event : Flywheel::SyncEvent::None);

		// End the scan if necessary.
		if(next_scan) {
			next_scan->end_points[1] = end_point(uint16_t((total_cycles - number_of_cycles) * number_of_samples / total_cycles));
			scan_target_->end_scan();
		}

		// Announce horizontal retrace events.
		if(next_run_length == time_until_horizontal_sync_event && next_horizontal_sync_event != Flywheel::SyncEvent::None) {
			// Reset the cycles-since-sync counter if this is the end of retrace.
			if(next_horizontal_sync_event == Flywheel::SyncEvent::EndRetrace) {
				cycles_since_horizontal_sync_ = 0;

				// This is unnecessary, strictly speaking, but seeks to help ScanTargets fit as
				// much as possible into a fixed range.
				phase_numerator_ %= phase_denominator_;
				if(!phase_numerator_) phase_numerator_ += phase_denominator_;
			}

			// Announce event.
			const auto event =
				(next_horizontal_sync_event == Flywheel::SyncEvent::StartRetrace)
					? Outputs::Display::ScanTarget::Event::BeginHorizontalRetrace : Outputs::Display::ScanTarget::Event::EndHorizontalRetrace;
			scan_target_->announce(
				event,
				!(horizontal_flywheel_->is_in_retrace() || vertical_flywheel_->is_in_retrace()),
				end_point(uint16_t((total_cycles - number_of_cycles) * number_of_samples / total_cycles)),
				colour_burst_amplitude_);

			// If retrace is starting, update phase if required and mark no colour burst spotted yet.
			if(next_horizontal_sync_event == Flywheel::SyncEvent::StartRetrace) {
				is_alernate_line_ ^= phase_alternates_;
				colour_burst_amplitude_ = 0;
			}
		}

		// Also announce vertical retrace events.
		if(next_run_length == time_until_vertical_sync_event && next_vertical_sync_event != Flywheel::SyncEvent::None) {
			const auto event =
				(next_vertical_sync_event == Flywheel::SyncEvent::StartRetrace)
					? Outputs::Display::ScanTarget::Event::BeginVerticalRetrace : Outputs::Display::ScanTarget::Event::EndVerticalRetrace;
			scan_target_->announce(
				event,
				!(horizontal_flywheel_->is_in_retrace() || vertical_flywheel_->is_in_retrace()),
				end_point(uint16_t((total_cycles - number_of_cycles) * number_of_samples / total_cycles)),
				colour_burst_amplitude_);
		}

		// if this is vertical retrace then adcance a field
		if(next_run_length == time_until_vertical_sync_event && next_vertical_sync_event == Flywheel::SyncEvent::EndRetrace) {
			if(delegate_) {
				frames_since_last_delegate_call_++;
				if(frames_since_last_delegate_call_ == 20) {
					delegate_->crt_did_end_batch_of_frames(this, frames_since_last_delegate_call_, vertical_flywheel_->get_and_reset_number_of_surprises());
					frames_since_last_delegate_call_ = 0;
				}
			}
		}
	}

	if(did_output) {
		scan_target_->submit();
	}
}

// MARK: - stream feeding methods

void CRT::output_scan(const Scan *const scan) {
	// Simplified colour burst logic: if it's within the back porch we'll take it.
	if(scan->type == Scan::Type::ColourBurst) {
		if(!colour_burst_amplitude_ && horizontal_flywheel_->get_current_time() < (horizontal_flywheel_->get_standard_period() * 12) >> 6) {
			// Load phase_numerator_ as a fixed-point quantity in the range [0, 255].
			phase_numerator_ = scan->phase;
			if(colour_burst_phase_adjustment_ != 0xff)
				phase_numerator_ = (phase_numerator_ & ~63) + colour_burst_phase_adjustment_;

			// Multiply the phase_numerator_ up to be to the proper scale.
			phase_numerator_ = (phase_numerator_ * phase_denominator_) >> 8;

			// Crib the colour burst amplitude.
			colour_burst_amplitude_ = scan->amplitude;
		}
	}
	// TODO: inspect raw data for potential colour burst if required; the DPLL and some zero crossing logic
	// will probably be sufficient but some test data would be helpful

	// sync logic: mark whether this is currently sync and check for a leading edge
	const bool this_is_sync = (scan->type == Scan::Type::Sync);
	const bool is_leading_edge = (!is_receiving_sync_ && this_is_sync);
	is_receiving_sync_ = this_is_sync;

	// Horizontal sync is recognised on any leading edge that is not 'near' the expected vertical sync;
	// the second limb is to avoid slightly horizontal sync shifting from the common pattern of
	// equalisation pulses as the inverse of ordinary horizontal sync.
	bool hsync_requested = is_leading_edge && !vertical_flywheel_->is_near_expected_sync();

	if(this_is_sync) {
		// If this is sync then either begin or continue a sync accumulation phase.
		is_accumulating_sync_ = true;
		cycles_since_sync_ = 0;
	} else {
		// If this is not sync then check how long it has been since sync. If it's more than
		// half a line then end sync accumulation and zero out the accumulating count.
		cycles_since_sync_ += scan->number_of_cycles;
		if(cycles_since_sync_ > (cycles_per_line_ >> 2)) {
			cycles_of_sync_ = 0;
			is_accumulating_sync_ = false;
			is_refusing_sync_ = false;
		}
	}

	int number_of_cycles = scan->number_of_cycles;
	bool vsync_requested = false;

	// If sync is being accumulated then accumulate it; if it crosses the vertical sync threshold then
	// divide this line at the crossing point and indicate vertical sync there.
	if(is_accumulating_sync_ && !is_refusing_sync_) {
		cycles_of_sync_ += scan->number_of_cycles;

		if(this_is_sync && cycles_of_sync_ >= sync_capacitor_charge_threshold_) {
			const int overshoot = std::min(cycles_of_sync_ - sync_capacitor_charge_threshold_, number_of_cycles);
			if(overshoot) {
				number_of_cycles -= overshoot;
				advance_cycles(number_of_cycles, hsync_requested, false, scan->type, 0);
				hsync_requested = false;
				number_of_cycles = overshoot;
			}

			is_refusing_sync_ = true;
			vsync_requested = true;
		}
	}

	advance_cycles(number_of_cycles, hsync_requested, vsync_requested, scan->type, scan->number_of_samples);
}

/*
	These all merely channel into advance_cycles, supplying appropriate arguments
*/
void CRT::output_sync(int number_of_cycles) {
	Scan scan;
	scan.type = Scan::Type::Sync;
	scan.number_of_cycles = number_of_cycles;
	output_scan(&scan);
}

void CRT::output_blank(int number_of_cycles) {
	Scan scan;
	scan.type = Scan::Type::Blank;
	scan.number_of_cycles = number_of_cycles;
	output_scan(&scan);
}

void CRT::output_level(int number_of_cycles) {
	scan_target_->end_data(1);
	Scan scan;
	scan.type = Scan::Type::Level;
	scan.number_of_cycles = number_of_cycles;
	scan.number_of_samples = 1;
	output_scan(&scan);
}

void CRT::output_colour_burst(int number_of_cycles, uint8_t phase, uint8_t amplitude) {
	Scan scan;
	scan.type = Scan::Type::ColourBurst;
	scan.number_of_cycles = number_of_cycles;
	scan.phase = phase;
	scan.amplitude = amplitude >> 1;
	output_scan(&scan);
}

void CRT::output_default_colour_burst(int number_of_cycles, uint8_t amplitude) {
	// TODO: avoid applying a rounding error here?
	output_colour_burst(number_of_cycles, static_cast<uint8_t>((phase_numerator_ * 256) / phase_denominator_), amplitude);
}

void CRT::set_immediate_default_phase(float phase) {
	phase = fmodf(phase, 1.0f);
	phase_numerator_ = static_cast<int>(phase * static_cast<float>(phase_denominator_));
}

void CRT::output_data(int number_of_cycles, size_t number_of_samples) {
#ifndef NDEBUG
	assert(number_of_samples > 0 && number_of_samples <= allocated_data_length_);
	allocated_data_length_ = std::numeric_limits<size_t>::min();
#endif
	scan_target_->end_data(number_of_samples);
	Scan scan;
	scan.type = Scan::Type::Data;
	scan.number_of_cycles = number_of_cycles;
	scan.number_of_samples = int(number_of_samples);
	output_scan(&scan);
}

Outputs::Display::Rect CRT::get_rect_for_area(int first_line_after_sync, int number_of_lines, int first_cycle_after_sync, int number_of_cycles, float aspect_ratio) {
	first_cycle_after_sync *= time_multiplier_;
	number_of_cycles *= time_multiplier_;

	first_line_after_sync -= 2;
	number_of_lines += 4;

	// determine prima facie x extent
	const int horizontal_period = horizontal_flywheel_->get_standard_period();
	const int horizontal_scan_period = horizontal_flywheel_->get_scan_period();
	const int horizontal_retrace_period = horizontal_period - horizontal_scan_period;

	// make sure that the requested range is visible
	if(static_cast<int>(first_cycle_after_sync) < horizontal_retrace_period) first_cycle_after_sync = static_cast<int>(horizontal_retrace_period);
	if(static_cast<int>(first_cycle_after_sync + number_of_cycles) > horizontal_scan_period) number_of_cycles = static_cast<int>(horizontal_scan_period - static_cast<int>(first_cycle_after_sync));

	float start_x = static_cast<float>(static_cast<int>(first_cycle_after_sync) - horizontal_retrace_period) / static_cast<float>(horizontal_scan_period);
	float width = static_cast<float>(number_of_cycles) / static_cast<float>(horizontal_scan_period);

	// determine prima facie y extent
	const int vertical_period = vertical_flywheel_->get_standard_period();
	const int vertical_scan_period = vertical_flywheel_->get_scan_period();
	const int vertical_retrace_period = vertical_period - vertical_scan_period;

	// make sure that the requested range is visible
//	if(static_cast<int>(first_line_after_sync) * horizontal_period < vertical_retrace_period)
//		first_line_after_sync = (vertical_retrace_period + horizontal_period - 1) / horizontal_period;
//	if((first_line_after_sync + number_of_lines) * horizontal_period > vertical_scan_period)
//		number_of_lines = static_cast<int>(horizontal_scan_period - static_cast<int>(first_cycle_after_sync));

	float start_y = static_cast<float>((static_cast<int>(first_line_after_sync) * horizontal_period) - vertical_retrace_period) / static_cast<float>(vertical_scan_period);
	float height = static_cast<float>(static_cast<int>(number_of_lines) * horizontal_period) / vertical_scan_period;

	// adjust to ensure aspect ratio is correct
	const float adjusted_aspect_ratio = (3.0f*aspect_ratio / 4.0f);
	const float ideal_width = height * adjusted_aspect_ratio;
	if(ideal_width > width) {
		start_x -= (ideal_width - width) * 0.5f;
		width = ideal_width;
	} else {
		float ideal_height = width / adjusted_aspect_ratio;
		start_y -= (ideal_height - height) * 0.5f;
		height = ideal_height;
	}

	return Outputs::Display::Rect(start_x, start_y, width, height);
}
