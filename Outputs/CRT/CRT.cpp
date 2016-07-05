//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include "CRTOpenGL.hpp"
#include <stdarg.h>
#include <math.h>
#include <algorithm>

using namespace Outputs::CRT;

void CRT::set_new_timing(unsigned int cycles_per_line, unsigned int height_of_display, ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator)
{
	_openGL_output_builder->set_colour_format(colour_space, colour_cycle_numerator, colour_cycle_denominator);

	const unsigned int syncCapacityLineChargeThreshold = 2;
	const unsigned int millisecondsHorizontalRetraceTime = 7;	// source: Dictionary of Video and Television Technology, p. 234
	const unsigned int scanlinesVerticalRetraceTime = 10;		// source: ibid

																// To quote:
																//
																//	"retrace interval; The interval of time for the return of the blanked scanning beam of
																//	a TV picture tube or camera tube to the starting point of a line or field. It is about 7 µs
																//	for horizontal retrace and 500 to 750 µs for vertical retrace in NTSC and PAL TV."

	_time_multiplier = IntermediateBufferWidth / cycles_per_line;

	// store fundamental display configuration properties
	_height_of_display = height_of_display;
	_cycles_per_line = cycles_per_line * _time_multiplier;

	// generate timing values implied by the given arbuments
	_sync_capacitor_charge_threshold = (int)(syncCapacityLineChargeThreshold * _cycles_per_line);

	// create the two flywheels
	_horizontal_flywheel.reset(new Flywheel(_cycles_per_line, (millisecondsHorizontalRetraceTime * _cycles_per_line) >> 6, _cycles_per_line >> 6));
	_vertical_flywheel.reset(new Flywheel(_cycles_per_line * height_of_display, scanlinesVerticalRetraceTime * _cycles_per_line, (_cycles_per_line * height_of_display) >> 3));

	// figure out the divisor necessary to get the horizontal flywheel into a 16-bit range
	unsigned int real_clock_scan_period = (_cycles_per_line * height_of_display) / (_time_multiplier * _common_output_divisor);
	_vertical_flywheel_output_divider = (uint16_t)(ceilf(real_clock_scan_period / 65536.0f) * (_time_multiplier * _common_output_divisor));

	_openGL_output_builder->set_timing(cycles_per_line, _cycles_per_line, _height_of_display, _horizontal_flywheel->get_scan_period(), _vertical_flywheel->get_scan_period(), _vertical_flywheel_output_divider);
}

void CRT::set_new_display_type(unsigned int cycles_per_line, DisplayType displayType)
{
	switch(displayType)
	{
		case DisplayType::PAL50:
			set_new_timing(cycles_per_line, 312, ColourSpace::YUV, 1135, 4);
		break;

		case DisplayType::NTSC60:
			set_new_timing(cycles_per_line, 262, ColourSpace::YIQ, 545, 2);
		break;
	}
}

CRT::CRT(unsigned int common_output_divisor) :
	_sync_capacitor_charge_level(0),
	_is_receiving_sync(false),
	_sync_period(0),
	_common_output_divisor(common_output_divisor),
	_is_writing_composite_run(false),
	_delegate(nullptr),
	_frames_since_last_delegate_call(0) {}

CRT::CRT(unsigned int cycles_per_line, unsigned int common_output_divisor, unsigned int height_of_display, ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator, unsigned int buffer_depth) : CRT(common_output_divisor)
{
	_openGL_output_builder.reset(new OpenGLOutputBuilder(buffer_depth));
	set_new_timing(cycles_per_line, height_of_display, colour_space, colour_cycle_numerator, colour_cycle_denominator);
}

CRT::CRT(unsigned int cycles_per_line, unsigned int common_output_divisor, DisplayType displayType, unsigned int buffer_depth) : CRT(common_output_divisor)
{
	_openGL_output_builder.reset(new OpenGLOutputBuilder(buffer_depth));
	set_new_display_type(cycles_per_line, displayType);
}

#pragma mark - Sync loop

Flywheel::SyncEvent CRT::get_next_vertical_sync_event(bool vsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced)
{
	return _vertical_flywheel->get_next_event_in_period(vsync_is_requested, cycles_to_run_for, cycles_advanced);
}

Flywheel::SyncEvent CRT::get_next_horizontal_sync_event(bool hsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced)
{
	return _horizontal_flywheel->get_next_event_in_period(hsync_is_requested, cycles_to_run_for, cycles_advanced);
}

#define output_x1()			(*(uint16_t *)&next_run[OutputVertexOffsetOfHorizontal + 0])
#define output_x2()			(*(uint16_t *)&next_run[OutputVertexOffsetOfHorizontal + 2])
#define output_position_y()	(*(uint16_t *)&next_run[OutputVertexOffsetOfVertical + 0])
#define output_tex_y()		(*(uint16_t *)&next_run[OutputVertexOffsetOfVertical + 2])

#define source_input_position_x1()	(*(uint16_t *)&next_run[SourceVertexOffsetOfInputStart + 0])
#define source_input_position_y()	(*(uint16_t *)&next_run[SourceVertexOffsetOfInputStart + 2])
#define source_input_position_x2()	(*(uint16_t *)&next_run[SourceVertexOffsetOfEnds + 0])
#define source_output_position_x1()	(*(uint16_t *)&next_run[SourceVertexOffsetOfOutputStart + 0])
#define source_output_position_y()	(*(uint16_t *)&next_run[SourceVertexOffsetOfOutputStart + 2])
#define source_output_position_x2()	(*(uint16_t *)&next_run[SourceVertexOffsetOfEnds + 2])
#define source_phase()				next_run[SourceVertexOffsetOfPhaseTimeAndAmplitude + 0]
#define source_amplitude()			next_run[SourceVertexOffsetOfPhaseTimeAndAmplitude + 2]
#define source_phase_time()			next_run[SourceVertexOffsetOfPhaseTimeAndAmplitude + 1]

void CRT::advance_cycles(unsigned int number_of_cycles, unsigned int source_divider, bool hsync_requested, bool vsync_requested, const bool vsync_charging, const Scan::Type type, uint16_t tex_x, uint16_t tex_y)
{
	number_of_cycles *= _time_multiplier;

	bool is_output_run = ((type == Scan::Type::Level) || (type == Scan::Type::Data));

	while(number_of_cycles) {

		unsigned int time_until_vertical_sync_event, time_until_horizontal_sync_event;
		Flywheel::SyncEvent next_vertical_sync_event = get_next_vertical_sync_event(vsync_requested, number_of_cycles, &time_until_vertical_sync_event);
		Flywheel::SyncEvent next_horizontal_sync_event = get_next_horizontal_sync_event(hsync_requested, time_until_vertical_sync_event, &time_until_horizontal_sync_event);

		// get the next sync event and its timing; hsync request is instantaneous (being edge triggered) so
		// set it to false for the next run through this loop (if any)
		unsigned int next_run_length = std::min(time_until_vertical_sync_event, time_until_horizontal_sync_event);

		hsync_requested = false;
		vsync_requested = false;

		bool is_output_segment = ((is_output_run && next_run_length) && !_horizontal_flywheel->is_in_retrace() && !_vertical_flywheel->is_in_retrace());
		uint8_t *next_run = nullptr;
		if(is_output_segment && !_openGL_output_builder->composite_output_buffer_is_full())
		{
			next_run = _openGL_output_builder->get_next_source_run();
		}

		if(next_run)
		{
			source_input_position_x1() = tex_x;
			source_input_position_y() = tex_y;
			source_output_position_x1() = (uint16_t)_horizontal_flywheel->get_current_output_position();
			source_output_position_y() = _openGL_output_builder->get_composite_output_y();
			source_phase() = _colour_burst_phase;
			source_amplitude() = _colour_burst_amplitude;
			source_phase_time() = (uint8_t)_colour_burst_time; // assumption: burst was within the first 1/16 of the line
		}

		// decrement the number of cycles left to run for and increment the
		// horizontal counter appropriately
		number_of_cycles -= next_run_length;

		// either charge or deplete the vertical retrace capacitor (making sure it stops at 0)
		if(vsync_charging)
			_sync_capacitor_charge_level += next_run_length;
		else
			_sync_capacitor_charge_level = std::max(_sync_capacitor_charge_level - (int)next_run_length, 0);

		// react to the incoming event...
		_horizontal_flywheel->apply_event(next_run_length, (next_run_length == time_until_horizontal_sync_event) ? next_horizontal_sync_event : Flywheel::SyncEvent::None);
		_vertical_flywheel->apply_event(next_run_length, (next_run_length == time_until_vertical_sync_event) ? next_vertical_sync_event : Flywheel::SyncEvent::None);

		if(next_run)
		{
			// if this is a data run then advance the buffer pointer
			if(type == Scan::Type::Data && source_divider) tex_x += next_run_length / (_time_multiplier * source_divider);

			source_input_position_x2() = tex_x;
			source_output_position_x2() = (uint16_t)_horizontal_flywheel->get_current_output_position();

			_openGL_output_builder->complete_source_run();
		}

		// if this is horizontal retrace then advance the output line counter and bookend an output run
		Flywheel::SyncEvent honoured_event = Flywheel::SyncEvent::None;
		if(next_run_length == time_until_vertical_sync_event && next_vertical_sync_event != Flywheel::SyncEvent::None) honoured_event = next_vertical_sync_event;
		if(next_run_length == time_until_horizontal_sync_event && next_horizontal_sync_event != Flywheel::SyncEvent::None) honoured_event = next_horizontal_sync_event;
		bool needs_endpoint =
			(honoured_event == Flywheel::SyncEvent::StartRetrace && _is_writing_composite_run) ||
			(honoured_event == Flywheel::SyncEvent::EndRetrace && !_horizontal_flywheel->is_in_retrace() && !_vertical_flywheel->is_in_retrace());

		if(needs_endpoint)
		{
			if(
				_openGL_output_builder->composite_output_run_has_room_for_vertex() &&
				!_openGL_output_builder->composite_output_buffer_is_full())
			{
				if(!_is_writing_composite_run)
				{
					_output_run.x1 = (uint16_t)_horizontal_flywheel->get_current_output_position();
					_output_run.y = (uint16_t)(_vertical_flywheel->get_current_output_position() / _vertical_flywheel_output_divider);
					_output_run.tex_y = _openGL_output_builder->get_composite_output_y();
				}
				else
				{
					uint8_t *next_run = _openGL_output_builder->get_next_output_run();
					output_x1() = _output_run.x1;
					output_position_y() = _output_run.y;
					output_tex_y() = _output_run.tex_y;
					output_x2() = (uint16_t)_horizontal_flywheel->get_current_output_position();
					_openGL_output_builder->complete_output_run();
				}
				_is_writing_composite_run ^= true;
			}
		}

		if(next_run_length == time_until_horizontal_sync_event && next_horizontal_sync_event == Flywheel::SyncEvent::StartRetrace)
		{
			_openGL_output_builder->increment_composite_output_y();
		}

		// if this is vertical retrace then adcance a field
		if(next_run_length == time_until_vertical_sync_event && next_vertical_sync_event == Flywheel::SyncEvent::EndRetrace)
		{
			if(_delegate)
			{
				_frames_since_last_delegate_call++;
				if(_frames_since_last_delegate_call == 20)
				{
					// Yuck: to deal with the permitted ability of the delegate to make CRT changes that require the lock to be
					// asserted during the delegate call, temporarily release the lock. TODO: find a less blunt instrument.
					_openGL_output_builder->unlock_output();
					_delegate->crt_did_end_batch_of_frames(this, _frames_since_last_delegate_call, _vertical_flywheel->get_and_reset_number_of_surprises());
					_openGL_output_builder->lock_output();
					_frames_since_last_delegate_call = 0;
				}
			}
		}
	}
}

#undef output_x1
#undef output_x2
#undef output_position_y
#undef output_tex_y

#undef source_input_position_x1
#undef source_input_position_y
#undef source_input_position_x2
#undef source_output_position_x1
#undef source_output_position_y
#undef source_output_position_x2
#undef source_phase
#undef source_amplitude
#undef source_phase_time

#pragma mark - stream feeding methods

void CRT::output_scan(const Scan *const scan)
{
	const bool this_is_sync = (scan->type == Scan::Type::Sync);
	const bool is_trailing_edge = (_is_receiving_sync && !this_is_sync);
	const bool is_leading_edge = (!_is_receiving_sync && this_is_sync);
	_is_receiving_sync = this_is_sync;

	// This introduces a blackout period close to the expected vertical sync point in which horizontal syncs are not
	// recognised, effectively causing the horizontal flywheel to freewheel during that period. This attempts to seek
	// the problem that vertical sync otherwise often starts halfway through a scanline, which confuses the horizontal
	// flywheel. I'm currently unclear whether this is an accurate solution to this problem.
	const bool hsync_requested = is_leading_edge && !_vertical_flywheel->is_near_expected_sync();
	const bool vsync_requested = is_trailing_edge && (_sync_capacitor_charge_level >= _sync_capacitor_charge_threshold);

	// simplified colour burst logic: if it's within the back porch we'll take it
	if(scan->type == Scan::Type::ColourBurst)
	{
		if(_horizontal_flywheel->get_current_time() < (_horizontal_flywheel->get_standard_period() * 12) >> 6)
		{
			_colour_burst_time = (uint16_t)_horizontal_flywheel->get_current_time();
			_colour_burst_phase = scan->phase;
			_colour_burst_amplitude = scan->amplitude;
		}
	}

	// TODO: inspect raw data for potential colour burst if required

	_sync_period = _is_receiving_sync ? (_sync_period + scan->number_of_cycles) : 0;
	advance_cycles(scan->number_of_cycles, scan->source_divider, hsync_requested, vsync_requested, this_is_sync, scan->type, scan->tex_x, scan->tex_y);
}

/*
	These all merely channel into advance_cycles, supplying appropriate arguments
*/
void CRT::output_sync(unsigned int number_of_cycles)
{
	_openGL_output_builder->lock_output();
	Scan scan{
		.type = Scan::Type::Sync,
		.number_of_cycles = number_of_cycles
	};
	output_scan(&scan);
	_openGL_output_builder->unlock_output();
}

void CRT::output_blank(unsigned int number_of_cycles)
{
	_openGL_output_builder->lock_output();
	Scan scan {
		.type = Scan::Type::Blank,
		.number_of_cycles = number_of_cycles
	};
	output_scan(&scan);
	_openGL_output_builder->unlock_output();
}

void CRT::output_level(unsigned int number_of_cycles)
{
	_openGL_output_builder->lock_output();
	if(!_openGL_output_builder->input_buffer_is_full())
	{
		Scan scan {
			.type = Scan::Type::Level,
			.number_of_cycles = number_of_cycles,
			.tex_x = _openGL_output_builder->get_last_write_x_posititon(),
			.tex_y = _openGL_output_builder->get_last_write_y_posititon()
		};
		output_scan(&scan);
	}
	else
	{
		Scan scan {
			.type = Scan::Type::Blank,
			.number_of_cycles = number_of_cycles
		};
		output_scan(&scan);
	}
	_openGL_output_builder->unlock_output();
}

void CRT::output_colour_burst(unsigned int number_of_cycles, uint8_t phase, uint8_t amplitude)
{
	_openGL_output_builder->lock_output();
	Scan scan {
		.type = Scan::Type::ColourBurst,
		.number_of_cycles = number_of_cycles,
		.phase = phase,
		.amplitude = amplitude
	};
	output_scan(&scan);
	_openGL_output_builder->unlock_output();
}

void CRT::output_data(unsigned int number_of_cycles, unsigned int source_divider)
{
	_openGL_output_builder->lock_output();
	if(!_openGL_output_builder->input_buffer_is_full())
	{
		_openGL_output_builder->reduce_previous_allocation_to(number_of_cycles / source_divider);
		Scan scan {
			.type = Scan::Type::Data,
			.number_of_cycles = number_of_cycles,
			.tex_x = _openGL_output_builder->get_last_write_x_posititon(),
			.tex_y = _openGL_output_builder->get_last_write_y_posititon(),
			.source_divider = source_divider
		};
		output_scan(&scan);
	}
	else
	{
		Scan scan {
			.type = Scan::Type::Blank,
			.number_of_cycles = number_of_cycles
		};
		output_scan(&scan);
	}
	_openGL_output_builder->unlock_output();
}

Outputs::CRT::Rect CRT::get_rect_for_area(int first_line_after_sync, int number_of_lines, int first_cycle_after_sync, int number_of_cycles, float aspect_ratio)
{
	first_cycle_after_sync *= _time_multiplier;
	number_of_cycles *= _time_multiplier;
	number_of_lines++;

	// determine prima facie x extent
	unsigned int horizontal_period = _horizontal_flywheel->get_standard_period();
	unsigned int horizontal_scan_period = _horizontal_flywheel->get_scan_period();
	unsigned int horizontal_retrace_period = horizontal_period - horizontal_scan_period;

	// make sure that the requested range is visible
	if(first_cycle_after_sync < horizontal_retrace_period) first_cycle_after_sync = (int)horizontal_retrace_period;
	if(first_cycle_after_sync + number_of_cycles > horizontal_scan_period) number_of_cycles = (int)(horizontal_scan_period - (unsigned)first_cycle_after_sync);

	float start_x = (float)((unsigned)first_cycle_after_sync - horizontal_retrace_period) / (float)horizontal_scan_period;
	float width = (float)number_of_cycles / (float)horizontal_scan_period;

	// determine prima facie y extent
	unsigned int vertical_period = _vertical_flywheel->get_standard_period();
	unsigned int vertical_scan_period = _vertical_flywheel->get_scan_period();
	unsigned int vertical_retrace_period = vertical_period - vertical_scan_period;

	// make sure that the requested range is visible
//	if((unsigned)first_line_after_sync * horizontal_period < vertical_retrace_period)
//		first_line_after_sync = (vertical_retrace_period + horizontal_period - 1) / horizontal_period;
//	if((first_line_after_sync + number_of_lines) * horizontal_period > vertical_scan_period)
//		number_of_lines = (int)(horizontal_scan_period - (unsigned)first_cycle_after_sync);

	float start_y = (float)(((unsigned)first_line_after_sync * horizontal_period) - vertical_retrace_period) / (float)vertical_scan_period;
	float height = (float)((unsigned)number_of_lines * horizontal_period) / vertical_scan_period;

	// adjust to ensure aspect ratio is correct
	float adjusted_aspect_ratio = (3.0f*aspect_ratio / 4.0f);
	float ideal_width = height * adjusted_aspect_ratio;
	if(ideal_width > width)
	{
		start_x -= (ideal_width - width) * 0.5f;
		width = ideal_width;
	}
	else
	{
		float ideal_height = width / adjusted_aspect_ratio;
		start_y -= (ideal_height - height) * 0.5f;
		height = ideal_height;
	}

	return Rect(start_x, start_y, width, height);
}
