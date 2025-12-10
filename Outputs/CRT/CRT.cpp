//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"

#include "Outputs/Log.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <optional>

using namespace Outputs::CRT;
using Logger = Log::Logger<Log::Source::CRT>;

// MARK: - Input timing setup.

void CRT::set_new_timing(
	const int cycles_per_line,
	const int height_of_display,
	const Outputs::Display::ColourSpace colour_space,
	const int colour_cycle_numerator,
	const int colour_cycle_denominator,
	const int vertical_sync_half_lines,
	const bool should_alternate
) {
	static constexpr int HorizontalRetraceMs = 7;	// Source: Dictionary of Video and Television Technology, p. 234.
	static constexpr int VerticalRetraceLines = 8;	// Source: ibid.

		// To quote:
		//
		//	"retrace interval; The interval of time for the return of the blanked scanning beam of
		//	a TV picture tube or camera tube to the starting point of a line or field. It is about
		//	7 microseconds for horizontal retrace and 500 to 750 microseconds for vertical retrace
		//  in NTSC and PAL TV."


	const bool is_first_set = time_multiplier_ == 0;

	// 63475 = 65535 * 31/32, i.e. the same 1/32 error as below is permitted.
	time_multiplier_ = 63487 / cycles_per_line;

	phase_denominator_ = int64_t(cycles_per_line) * int64_t(colour_cycle_denominator) * int64_t(time_multiplier_);
	phase_numerator_ = 0;
	colour_cycle_numerator_ = int64_t(colour_cycle_numerator);
	phase_alternates_ = should_alternate;
	should_be_alternate_line_ &= phase_alternates_;
	cycles_per_line_ = cycles_per_line;

	const int multiplied_cycles_per_line = cycles_per_line * time_multiplier_;

	// Allow sync to be detected (and acted upon) a line earlier than the specified requirement,
	// as a simple way of avoiding not-quite-exact comparison issues while still being true enough to
	// the gist for simple debugging.
	sync_capacitor_charge_threshold_ = ((vertical_sync_half_lines - 2) * cycles_per_line) >> 1;

	// Horizontal flywheel: has an ideal period of `multiplied_cycles_per_line`, will accept syncs
	// within 1/32nd of that (i.e. tolerates 3.125% error) and takes HorizontalRetraceMs
	// to retrace.
	horizontal_flywheel_ =
		Flywheel(
			multiplied_cycles_per_line,
			(HorizontalRetraceMs * multiplied_cycles_per_line) >> 6,
			multiplied_cycles_per_line >> 5
		);

	// Vertical flywheel: has an ideal period of `multiplied_cycles_per_line * height_of_display`,
	// will accept syncs within 1/8th of that (i.e. tolerates 12.5% error) and takes VerticalRetraceLines
	// to retrace.
	vertical_flywheel_ =
		Flywheel(
			multiplied_cycles_per_line * height_of_display,
			VerticalRetraceLines * multiplied_cycles_per_line,
			(multiplied_cycles_per_line * height_of_display) >> 3
		);

	// Figure out the divisor necessary to get the horizontal flywheel into a 16-bit range.
	const int real_clock_scan_period = vertical_flywheel_.scan_period();
	vertical_flywheel_output_divider_ = (real_clock_scan_period + 65534) / 65535;

	// Communicate relevant fields to the scan target.
	scan_target_modals_.cycles_per_line = cycles_per_line;
	scan_target_modals_.output_scale.x = uint16_t(horizontal_flywheel_.scan_period());
	scan_target_modals_.output_scale.y = uint16_t(real_clock_scan_period / vertical_flywheel_output_divider_);
	scan_target_modals_.expected_vertical_lines = height_of_display;
	scan_target_modals_.composite_colour_space = colour_space;
	scan_target_modals_.colour_cycle_numerator = colour_cycle_numerator;
	scan_target_modals_.colour_cycle_denominator = colour_cycle_denominator;

	// Default crop: middle 90%.
	if(is_first_set) {
		posted_rect_ = scan_target_modals_.visible_area = Display::Rect(
			0.05f, 0.05f, 0.9f, 0.9f
		);
	}

	scan_target_->set_modals(scan_target_modals_);

	const float stability_threshold = 1.0f / scan_target_modals_.expected_vertical_lines;
	rect_accumulator_.set_stability_threshold(stability_threshold);
}

void CRT::set_dynamic_framing(
	Outputs::Display::Rect initial,
	float max_centre_offset_x,
	float max_centre_offset_y,
	float maximum_scale,
	float minimum_scale
) {
	framing_ = Framing::Dynamic;

	dynamic_framer_.framing_bounds = initial;
	dynamic_framer_.framing_bounds.scale(
		maximum_scale / dynamic_framer_.framing_bounds.size.width,
		maximum_scale / dynamic_framer_.framing_bounds.size.height
	);

	dynamic_framer_.minimum_scale = minimum_scale;
	dynamic_framer_.max_offsets[0] = max_centre_offset_x;
	dynamic_framer_.max_offsets[1] = max_centre_offset_y;

	if(!has_first_reading_) {
		previous_posted_rect_ = posted_rect_ = scan_target_modals_.visible_area = initial;
		scan_target_->set_modals(scan_target_modals_);
	}
	has_first_reading_ = true;
	animation_step_ = AnimationSteps;
}

void CRT::set_fixed_framing(const std::function<void()> &advance) {
	framing_ = Framing::CalibratingAutomaticFixed;
	while(framing_ == Framing::CalibratingAutomaticFixed) {
		advance();
	}
}

void CRT::set_fixed_framing(const Display::Rect frame) {
	framing_ = Framing::Static;
	static_frame_ = frame;
	if(!has_first_reading_) {
		scan_target_modals_.visible_area = frame;
		scan_target_->set_modals(scan_target_modals_);
	}
}

void CRT::set_new_display_type(const int cycles_per_line, const Outputs::Display::Type displayType) {
	switch(displayType) {
		case Outputs::Display::Type::PAL50:
		case Outputs::Display::Type::PAL60:
			scan_target_modals_.intended_gamma = 2.8f;
			set_new_timing(
				cycles_per_line,
				(displayType == Outputs::Display::Type::PAL50) ? 312 : 262,
				PAL::ColourSpace,
				PAL::ColourCycleNumerator,
				PAL::ColourCycleDenominator,
				PAL::VerticalSyncLength,
				PAL::AlternatesPhase);
		break;

		case Outputs::Display::Type::NTSC60:
			scan_target_modals_.intended_gamma = 2.2f;
			set_new_timing(
				cycles_per_line,
				262,
				NTSC::ColourSpace,
				NTSC::ColourCycleNumerator,
				NTSC::ColourCycleDenominator,
				NTSC::VerticalSyncLength,
				NTSC::AlternatesPhase);
		break;
	}
}

void CRT::set_composite_function_type(const CompositeSourceType type, const float offset_of_first_sample) {
	if(type == DiscreteFourSamplesPerCycle) {
		colour_burst_phase_adjustment_ = uint8_t(offset_of_first_sample * 256.0f) & 63;
	} else {
		colour_burst_phase_adjustment_ = 0xff;
	}
}

// MARK: - Constructors.

CRT::CRT() : animation_curve_(Numeric::CubicCurve::easeInOut()) {}

CRT::CRT(
	const int cycles_per_line,
	const int clocks_per_pixel_greatest_common_divisor,
	const int height_of_display,
	const Outputs::Display::ColourSpace colour_space,
	const int colour_cycle_numerator, int colour_cycle_denominator,
	const int vertical_sync_half_lines,
	const bool should_alternate,
	const Outputs::Display::InputDataType data_type
) : CRT() {
	scan_target_modals_.input_data_type = data_type;
	scan_target_modals_.clocks_per_pixel_greatest_common_divisor = clocks_per_pixel_greatest_common_divisor;
	set_new_timing(
		cycles_per_line,
		height_of_display,
		colour_space,
		colour_cycle_numerator,
		colour_cycle_denominator,
		vertical_sync_half_lines,
		should_alternate
	);
}

CRT::CRT(
	const int cycles_per_line,
	const int clocks_per_pixel_greatest_common_divisor,
	const Outputs::Display::Type display_type,
	const Outputs::Display::InputDataType data_type
) : CRT() {
	scan_target_modals_.input_data_type = data_type;
	scan_target_modals_.clocks_per_pixel_greatest_common_divisor = clocks_per_pixel_greatest_common_divisor;
	set_new_display_type(cycles_per_line, display_type);
}

CRT::CRT(
	const int cycles_per_line,
	const int clocks_per_pixel_greatest_common_divisor,
	const int height_of_display,
	const int vertical_sync_half_lines,
	const Outputs::Display::InputDataType data_type
) : CRT() {
	scan_target_modals_.input_data_type = data_type;
	scan_target_modals_.clocks_per_pixel_greatest_common_divisor = clocks_per_pixel_greatest_common_divisor;
	set_new_timing(
		cycles_per_line,
		height_of_display,
		Outputs::Display::ColourSpace::YIQ,
		1,
		1,
		vertical_sync_half_lines,
		false
	);
}

// Use some from-thin-air arbitrary constants for default timing, otherwise passing
// construction off to one of the other constructors.
CRT::CRT(const Outputs::Display::InputDataType data_type) : CRT(100, 1, 100, 1, data_type) {}

// MARK: - Sync loop

void CRT::advance_cycles(
	int number_of_cycles,
	bool hsync_requested,
	bool vsync_requested,
	const Scan::Type type,
	const int number_of_samples
) {
	number_of_cycles *= time_multiplier_;

	const bool is_output_run = type == Scan::Type::Level || type == Scan::Type::Data;
	const auto total_cycles = number_of_cycles;
	bool did_output = false;
	const auto end_point = [&] {
		return this->end_point(uint16_t((total_cycles - number_of_cycles) * number_of_samples / total_cycles));
	};

	using EndPoint = Outputs::Display::ScanTarget::Scan::EndPoint;
	EndPoint start_point;

	while(number_of_cycles) {
		// Get time until next horizontal and vertical sync generator events.
		const auto vertical_event = vertical_flywheel_.next_event_in_period(vsync_requested, number_of_cycles);
		assert(vertical_event.second >= 0 && vertical_event.second <= number_of_cycles);

		const auto horizontal_event = horizontal_flywheel_.next_event_in_period(hsync_requested, vertical_event.second);
		assert(horizontal_event.second >= 0 && horizontal_event.second <= vertical_event.second);

		// Whichever event is scheduled to happen first is the one to advance to.
		const int next_run_length = horizontal_event.second;

		// Request each sync at most once.
		hsync_requested = false;
		vsync_requested = false;

		// Determine whether to output any data for this portion of the output; if so then grab somewhere to put it.
		const bool is_output_segment =
			is_output_run &&
			next_run_length &&
			!horizontal_flywheel_.is_in_retrace() &&
			!vertical_flywheel_.is_in_retrace();
		Outputs::Display::ScanTarget::Scan *const next_scan = is_output_segment ? scan_target_->begin_scan() : nullptr;
		did_output |= is_output_segment;

		// If outputting, store the start location and scan constants.
		if(next_scan) {
			next_scan->end_points[0] = end_point();
			next_scan->composite_amplitude = colour_burst_amplitude_;
		} else if(is_output_segment /* && is_calibrating(framing_) */) {
			start_point = end_point();
		}

		// Advance time: that'll affect both the colour subcarrier and the number of cycles left to run.
		phase_numerator_ += next_run_length * colour_cycle_numerator_;
		number_of_cycles -= next_run_length;
		cycles_since_horizontal_sync_ += next_run_length;

		// React to the incoming event.
		horizontal_flywheel_.apply_event(
			next_run_length,
			next_run_length == horizontal_event.second ? horizontal_event.first : Flywheel::SyncEvent::None
		);

		const auto active_vertical_event =
			next_run_length == vertical_event.second ? vertical_event.first : Flywheel::SyncEvent::None;
		vertical_flywheel_.apply_event(next_run_length, active_vertical_event);

		if(active_vertical_event == Flywheel::SyncEvent::StartRetrace) {
			/* if(is_calibrating(framing_)) */ {
				active_rect_.origin.x /= scan_target_modals_.output_scale.x;
				active_rect_.size.width /= scan_target_modals_.output_scale.x;
				active_rect_.origin.y /= scan_target_modals_.output_scale.y;
				active_rect_.size.height /= scan_target_modals_.output_scale.y;

				border_rect_.origin.x /= scan_target_modals_.output_scale.x;
				border_rect_.size.width /= scan_target_modals_.output_scale.x;
				border_rect_.origin.y /= scan_target_modals_.output_scale.y;
				border_rect_.size.height /= scan_target_modals_.output_scale.y;
			}

			if(
				captures_in_rect_ > 5 &&
				active_rect_.size.width > 0.05f &&
				active_rect_.size.height > 0.05f &&
				vertical_flywheel_.was_stable()
			) {
				if(!level_changes_in_frame_) {
					posit(active_rect_);
				} else if(level_changes_in_frame_ < 20) {
					posit(active_rect_ * 0.9f + border_rect_ * 0.1f);
				} else {
					posit(active_rect_ * 0.3f + border_rect_ * 0.7f);
				}
			}
			level_changes_in_frame_ = 0;

			/* if(is_calibrating(framing_)) */ {
				border_rect_ = active_rect_ = Display::Rect(65536.0f, 65536.0f, 0.0f, 0.0f);
				captures_in_rect_ = 0;
			}
		}

		// End the scan if necessary.
		const auto posit_scan = [&](const EndPoint &start, const EndPoint &end) {
			++captures_in_rect_;
			border_rect_.expand(start.x, end.x, start.y, end.y);
			if(number_of_samples > 1) {
				active_rect_.expand(start.x, end.x, start.y, end.y);
			}
		};

		if(next_scan) {
			next_scan->end_points[1] = end_point();
			posit_scan(next_scan->end_points[0], next_scan->end_points[1]);
			scan_target_->end_scan();
		} else if(is_output_segment) {
			posit_scan(start_point, end_point());
		}

		using Event = Outputs::Display::ScanTarget::Event;

		// Announce horizontal sync events.
		if(next_run_length == horizontal_event.second && horizontal_event.first != Flywheel::SyncEvent::None) {
			// Reset the cycles-since-sync counter if this is the end of retrace.
			if(horizontal_event.first == Flywheel::SyncEvent::EndRetrace) {
				cycles_since_horizontal_sync_ = 0;

				// This is unnecessary, strictly speaking, but seeks to help ScanTargets fit as
				// much as possible into a fixed range.
				phase_numerator_ %= phase_denominator_;
				if(!phase_numerator_) phase_numerator_ += phase_denominator_;
			}

			// Announce event.
			const auto event =
				horizontal_event.first == Flywheel::SyncEvent::StartRetrace
					? Event::BeginHorizontalRetrace : Event::EndHorizontalRetrace;
			scan_target_->announce(
				event,
				!(horizontal_flywheel_.is_in_retrace() || vertical_flywheel_.is_in_retrace()),
				end_point(),
				colour_burst_amplitude_);

			// If retrace is starting, update phase if required and mark no colour burst spotted yet.
			if(horizontal_event.first == Flywheel::SyncEvent::StartRetrace) {
				should_be_alternate_line_ ^= phase_alternates_;
				colour_burst_amplitude_ = 0;
			}
		}

		// Announce vertical sync events.
		if(next_run_length == vertical_event.second && vertical_event.first != Flywheel::SyncEvent::None) {
			const auto event =
				vertical_event.first == Flywheel::SyncEvent::StartRetrace
					? Event::BeginVerticalRetrace : Event::EndVerticalRetrace;
			scan_target_->announce(
				event,
				!(horizontal_flywheel_.is_in_retrace() || vertical_flywheel_.is_in_retrace()),
				end_point(),
				colour_burst_amplitude_);
		}

		// At vertical retrace advance a field.
		if(next_run_length == vertical_event.second && vertical_event.first == Flywheel::SyncEvent::EndRetrace) {
			if(delegate_) {
				++frames_since_last_delegate_call_;
				if(frames_since_last_delegate_call_ == 20) {
					delegate_->crt_did_end_batch_of_frames(
						*this,
						frames_since_last_delegate_call_,
						vertical_flywheel_.get_and_reset_number_of_surprises()
					);
					frames_since_last_delegate_call_ = 0;
				}
			}
		}
	}

	if(did_output) {
		scan_target_->submit();
	}
}

Outputs::Display::ScanTarget::Scan::EndPoint CRT::end_point(const uint16_t data_offset) {
	// Ensure .composite_angle is sampled at the location indicated by .cycles_since_end_of_horizontal_retrace.
	// TODO: I could supply time_multiplier_ as a modal and just not round .cycles_since_end_of_horizontal_retrace.
	// Would that be better?
	const auto lost_precision = cycles_since_horizontal_sync_ % time_multiplier_;
	const auto composite_angle =
		(((phase_numerator_ - lost_precision * colour_cycle_numerator_) << 6) / phase_denominator_)
			* (is_alternate_line_ ? -1 : 1);

	return Display::ScanTarget::Scan::EndPoint{
		// Clamp the available range on endpoints. These will almost always be within range, but may go
		// out during times of resync.
		.x = uint16_t(std::min(horizontal_flywheel_.current_output_position(), 65535)),
		.y = uint16_t(
			std::min(vertical_flywheel_.current_output_position() / vertical_flywheel_output_divider_, 65535)
		),
		.data_offset = data_offset,

		.composite_angle = int16_t(composite_angle),
		.cycles_since_end_of_horizontal_retrace = uint16_t(cycles_since_horizontal_sync_ / time_multiplier_),
	};
}

void CRT::posit(Display::Rect rect) {
	// Scale and push a rect.
	const auto set_rect = [&](const Display::Rect &rect) {
		scan_target_modals_.visible_area = rect;
		scan_target_->set_modals(scan_target_modals_);
	};

	// Get current interpolation between previous_posted_rect_ and posted_rect_.
	const auto current_rect = [&] {
		const auto animation_time = animation_curve_.value(float(animation_step_) / float(AnimationSteps));
		return
			previous_posted_rect_ * (1.0f - animation_time) +
			posted_rect_ * animation_time;
	};

	// Continue with any ongoing animation.
	if(animation_step_ != NoFrameYet && animation_step_ < AnimationSteps) {
		set_rect(current_rect());
		++animation_step_;
		if(animation_step_ == AnimationSteps) {
			previous_posted_rect_ = posted_rect_;
		}
	}

	// Zoom out very slightly if there's space; this avoids a cramped tight crop.
	if(rect.size.width < 0.95 && rect.size.height < 0.95) {
		rect.scale(1.02f, 1.02f);
	}

	std::optional<Display::Rect> first_reading;
	if(!has_first_reading_) {
		rect_accumulator_.posit(rect);
		if(first_reading = rect_accumulator_.first_reading(); first_reading.has_value()) {
			has_first_reading_ = true;
#ifndef NDEBUG
			Logger::info().append("First reading is (%0.5ff, %0.5ff, %0.5ff, %0.5ff)",
				posted_rect_.origin.x, posted_rect_.origin.y,
				posted_rect_.size.width, posted_rect_.size.height);

			auto frame = border_rect_;
			frame.scale(0.90f, 0.90f);
			Logger::info().append("90%% of whole frame was (%0.5ff, %0.5ff, %0.5ff, %0.5ff)",
				frame.origin.x, frame.origin.y,
				frame.size.width, frame.size.height);
#endif

			if(framing_ == Framing::CalibratingAutomaticFixed) {
				static_frame_ = *first_reading;
				framing_ =
					border_rect_ != active_rect_ ?
						Framing::BorderReactive : Framing::Static;
				return;
			}
		}

		return;
	}

	const auto output_frame = rect_accumulator_.posit(rect);
	dynamic_framer_.update(rect, output_frame, first_reading);

	const auto selected_rect = [&]() -> std::optional<Display::Rect> {
		switch(framing_) {
			default:						return rect;
			case Framing::Static:			return static_frame_;
			case Framing::Dynamic:			return dynamic_framer_.selection;
		}
	} ();

	if(selected_rect && *selected_rect != posted_rect_) {
		if(animation_step_ == NoFrameYet) {
			animation_step_ = AnimationSteps;
			previous_posted_rect_ = posted_rect_ = *selected_rect;
			set_rect(posted_rect_);
		} else {
			previous_posted_rect_ = current_rect();
			posted_rect_ = *selected_rect;
			animation_step_ = 0;
		}
	}
}

// MARK: - Stream feeding.

void CRT::output_scan(const Scan &scan) {
	assert(scan.number_of_cycles >= 0);

	// Simplified colour burst logic: if it's within the back porch we'll take it.
	if(scan.type == Scan::Type::ColourBurst) {
		if(
			!colour_burst_amplitude_ &&
			horizontal_flywheel_.current_time() < (horizontal_flywheel_.standard_period() * 12) >> 6
		) {
			// Load phase_numerator_ as a fixed-point quantity in the range [0, 255].
			phase_numerator_ = scan.phase;
			if(colour_burst_phase_adjustment_ != 0xff)
				phase_numerator_ = (phase_numerator_ & ~63) + colour_burst_phase_adjustment_;

			// Multiply the phase_numerator_ up to be to the proper scale.
			phase_numerator_ = (phase_numerator_ * phase_denominator_) >> 8;

			// Crib the colour burst amplitude.
			colour_burst_amplitude_ = scan.amplitude;
		}
	}
	// TODO: inspect raw data for potential colour burst if required; the DPLL and some zero crossing logic
	// will probably be sufficient but some test data would be helpful

	// sync logic: mark whether this is currently sync and check for a leading edge
	const bool this_is_sync = scan.type == Scan::Type::Sync;
	const bool is_leading_edge = !is_receiving_sync_ && this_is_sync;
	is_receiving_sync_ = this_is_sync;

	// Horizontal sync is recognised on any leading edge that is not 'near' the expected vertical sync;
	// the second limb is to avoid slightly horizontal sync shifting from the common pattern of
	// equalisation pulses as the inverse of ordinary horizontal sync.
	bool hsync_requested = is_leading_edge && !vertical_flywheel_.is_near_expected_sync();

	if(this_is_sync) {
		// If this is sync then either begin or continue a sync accumulation phase.
		is_accumulating_sync_ = true;
		cycles_since_sync_ = 0;
	} else {
		// If this is not sync then check how long it has been since sync. If it's more than
		// half a line then end sync accumulation and zero out the accumulating count.
		cycles_since_sync_ += scan.number_of_cycles;
		if(cycles_since_sync_ > (cycles_per_line_ >> 2)) {
			cycles_of_sync_ = 0;
			is_accumulating_sync_ = false;
			is_refusing_sync_ = false;
		}
	}

	int number_of_cycles = scan.number_of_cycles;
	bool vsync_requested = false;

	// If sync is being accumulated then accumulate it; if it crosses the vertical sync threshold then
	// divide this line at the crossing point and indicate vertical sync there.
	if(is_accumulating_sync_ && !is_refusing_sync_) {
		cycles_of_sync_ += scan.number_of_cycles;

		if(this_is_sync && cycles_of_sync_ >= sync_capacitor_charge_threshold_) {
			const int overshoot = std::min(cycles_of_sync_ - sync_capacitor_charge_threshold_, number_of_cycles);
			if(overshoot) {
				number_of_cycles -= overshoot;
				advance_cycles(number_of_cycles, hsync_requested, false, scan.type, 0);
				hsync_requested = false;
				number_of_cycles = overshoot;
			}

			is_refusing_sync_ = true;
			vsync_requested = true;
		}
	}

	advance_cycles(number_of_cycles, hsync_requested, vsync_requested, scan.type, scan.number_of_samples);
}

/*
	These all merely channel into advance_cycles, supplying appropriate arguments
*/
void CRT::output_sync(const int number_of_cycles) {
	output_scan(Scan{
		.type = Scan::Type::Sync,
		.number_of_cycles = number_of_cycles,
	});
}

void CRT::output_blank(const int number_of_cycles) {
	output_scan(Scan{
		.type = Scan::Type::Blank,
		.number_of_cycles = number_of_cycles,
	});
}

void CRT::output_level(const int number_of_cycles) {
	scan_target_->end_data(1);
	output_scan(Scan{
		.type = Scan::Type::Level,
		.number_of_cycles = number_of_cycles,
		.number_of_samples = 1,
	});
}

void CRT::output_colour_burst(
	const int number_of_cycles,
	const uint8_t phase,
	const bool is_alternate_line,
	const uint8_t amplitude
) {
	is_alternate_line_ = is_alternate_line;
	output_scan(Scan{
		.type = Scan::Type::ColourBurst,
		.number_of_cycles = number_of_cycles,
		.phase = phase,
		.amplitude = uint8_t(amplitude >> 1),
	});
}

void CRT::output_default_colour_burst(const int number_of_cycles, const uint8_t amplitude) {
	// TODO: avoid applying a rounding error here?
	output_colour_burst(
		number_of_cycles,
		uint8_t((phase_numerator_ * 256) / phase_denominator_),
		should_be_alternate_line_,
		amplitude
	);
}

void CRT::set_immediate_default_phase(const float phase) {
	phase_numerator_ = int(std::fmod(phase, 1.0f) * float(phase_denominator_));
}

void CRT::output_data(const int number_of_cycles, const size_t number_of_samples) {
#ifndef NDEBUG
//	assert(number_of_samples > 0);
//	assert(number_of_samples <= allocated_data_length_);
//	allocated_data_length_ = std::numeric_limits<size_t>::min();
#endif
	scan_target_->end_data(number_of_samples);
	output_scan(Scan{
		.type = Scan::Type::Data,
		.number_of_cycles = number_of_cycles,
		.number_of_samples = int(number_of_samples),
	});
}


// MARK: - Getters.

Outputs::Display::Rect CRT::get_rect_for_area(
	[[maybe_unused]] int first_line_after_sync,
	[[maybe_unused]] int number_of_lines,
	[[maybe_unused]] int first_cycle_after_sync,
	[[maybe_unused]] int number_of_cycles
) const {
	assert(number_of_cycles > 0);
	assert(number_of_lines > 0);
	assert(first_line_after_sync >= 0);
	assert(first_cycle_after_sync >= 0);

	// Scale up x coordinates and add a little extra leeway to y.
	first_cycle_after_sync *= time_multiplier_;
	number_of_cycles *= time_multiplier_;

	first_line_after_sync -= 2;
	number_of_lines += 4;

	// Determine prima facie x extent.
	const int horizontal_period = horizontal_flywheel_.standard_period();
	const int horizontal_scan_period = horizontal_flywheel_.scan_period();
	const int horizontal_retrace_period = horizontal_period - horizontal_scan_period;

	// Ensure requested range is within visible region.
	first_cycle_after_sync = std::max(horizontal_retrace_period, first_cycle_after_sync);
	number_of_cycles = std::min(horizontal_period - first_cycle_after_sync, number_of_cycles);

	float start_x = float(first_cycle_after_sync - horizontal_retrace_period) / float(horizontal_scan_period);
	float width = float(number_of_cycles) / float(horizontal_scan_period);

	// Determine prima facie y extent.
	const int vertical_period = vertical_flywheel_.standard_period();
	const int vertical_scan_period = vertical_flywheel_.scan_period();
	const int vertical_retrace_period = vertical_period - vertical_scan_period;

	// Ensure range is visible.
	first_line_after_sync = std::max(
		first_line_after_sync * horizontal_period,
		vertical_retrace_period
	) / horizontal_period;
	number_of_lines = std::min(
		vertical_period - first_line_after_sync * horizontal_period,
		number_of_lines * horizontal_period
	) / horizontal_period;

	const float start_y =
		float(first_line_after_sync * horizontal_period - vertical_retrace_period) /
		float(vertical_scan_period);
	const float height = float(number_of_lines * horizontal_period) / vertical_scan_period;

	return Outputs::Display::Rect(start_x, start_y, width, height);
}

Outputs::Display::ScanStatus CRT::get_scaled_scan_status() const {
	return Outputs::Display::ScanStatus{
		.field_duration = float(vertical_flywheel_.locked_period()) / float(time_multiplier_),
		.field_duration_gradient = float(vertical_flywheel_.last_period_adjustment()) / float(time_multiplier_),
		.retrace_duration = float(vertical_flywheel_.retrace_period()) / float(time_multiplier_),
		.current_position = float(vertical_flywheel_.current_phase()) / float(vertical_flywheel_.locked_scan_period()),
		.hsync_count = vertical_flywheel_.number_of_retraces(),
	};
}

// MARK: - ScanTarget passthroughs.

void CRT::set_scan_target(Outputs::Display::ScanTarget *const scan_target) {
	scan_target_ = scan_target;
	if(!scan_target_) scan_target_ = &Outputs::Display::NullScanTarget::singleton;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_new_data_type(const Outputs::Display::InputDataType data_type) {
	scan_target_modals_.input_data_type = data_type;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_aspect_ratio(const float aspect_ratio) {
	scan_target_modals_.aspect_ratio = aspect_ratio;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_display_type(const Outputs::Display::DisplayType display_type) {
	scan_target_modals_.display_type = display_type;
	scan_target_->set_modals(scan_target_modals_);
}

Outputs::Display::DisplayType CRT::get_display_type() const {
	return scan_target_modals_.display_type;
}

void CRT::set_phase_linked_luminance_offset(const float offset) {
	scan_target_modals_.input_data_tweaks.phase_linked_luminance_offset = offset;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_input_data_type(const Outputs::Display::InputDataType input_data_type) {
	scan_target_modals_.input_data_type = input_data_type;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_brightness(const float brightness) {
	scan_target_modals_.brightness = brightness;
	scan_target_->set_modals(scan_target_modals_);
}

void CRT::set_input_gamma(const float gamma) {
	scan_target_modals_.intended_gamma = gamma;
	scan_target_->set_modals(scan_target_modals_);
}
