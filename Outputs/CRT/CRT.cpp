//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include <stdarg.h>
#include <math.h>

using namespace Outputs;

static const uint32_t kCRTFixedPointRange	= 0xf7ffffff;
static const uint32_t kCRTFixedPointOffset	= 0x04000000;

//static const size_t kCRTVertexOffsetOfPosition = 0;
//static const size_t kCRTVertexOffsetOfTexCoord = 4;
//static const size_t kCRTVertexOffsetOfLateral = 8;
//static const size_t kCRTVertexOffsetOfPhase = 9;
//
//static const int kCRTSizeOfVertex = 10;

#define kRetraceXMask	0x01
#define kRetraceYMask	0x02

void CRT::set_new_timing(unsigned int cycles_per_line, unsigned int height_of_display, ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator)
{
	_colour_space = colour_space;
	_colour_cycle_numerator = colour_cycle_numerator;
	_colour_cycle_denominator = colour_cycle_denominator;

	const unsigned int syncCapacityLineChargeThreshold = 3;
	const unsigned int millisecondsHorizontalRetraceTime = 7;	// source: Dictionary of Video and Television Technology, p. 234
	const unsigned int scanlinesVerticalRetraceTime = 10;		// source: ibid

																// To quote:
																//
																//	"retrace interval; The interval of time for the return of the blanked scanning beam of
																//	a TV picture tube or camera tube to the starting point of a line or field. It is about 7 µs
																//	for horizontal retrace and 500 to 750 µs for vertical retrace in NTSC and PAL TV."

	_time_multiplier = (1000 + cycles_per_line - 1) / cycles_per_line;

	// store fundamental display configuration properties
	_height_of_display = height_of_display;
	_cycles_per_line = cycles_per_line * _time_multiplier;

	// generate timing values implied by the given arbuments
	_sync_capacitor_charge_threshold = ((syncCapacityLineChargeThreshold * _cycles_per_line) * 50) >> 7;
	const unsigned int vertical_retrace_time = scanlinesVerticalRetraceTime * _cycles_per_line;
	const float halfLineWidth = (float)_height_of_display * 2.0f;

	// creat the two flywheels
	unsigned int horizontal_retrace_time = scanlinesVerticalRetraceTime * _cycles_per_line;
	_horizontal_flywheel	= std::unique_ptr<Outputs::Flywheel>(new Outputs::Flywheel(_cycles_per_line, (millisecondsHorizontalRetraceTime * _cycles_per_line) >> 6));
	_vertical_flywheel		= std::unique_ptr<Outputs::Flywheel>(new Outputs::Flywheel(_cycles_per_line * height_of_display, scanlinesVerticalRetraceTime * _cycles_per_line));

	for(int c = 0; c < 4; c++)
	{
		_scanSpeed[c].x = (c&kRetraceXMask) ? -(kCRTFixedPointRange / horizontal_retrace_time)	: (kCRTFixedPointRange / _cycles_per_line);
		_scanSpeed[c].y = (c&kRetraceYMask) ? -(kCRTFixedPointRange / vertical_retrace_time)	: (kCRTFixedPointRange / (_height_of_display * _cycles_per_line));

		// width should be 1.0 / _height_of_display, rotated to match the direction
		float angle = atan2f(_scanSpeed[c].y, _scanSpeed[c].x);
		_beamWidth[c].x = (uint32_t)((sinf(angle) / halfLineWidth) * kCRTFixedPointRange);
		_beamWidth[c].y = (uint32_t)((cosf(angle) / halfLineWidth) * kCRTFixedPointRange);
	}
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

void CRT::allocate_buffers(unsigned int number, va_list sizes)
{
	// generate buffers for signal storage as requested — format is
	// number of buffers, size of buffer 1, size of buffer 2...
	const uint16_t bufferWidth = 2048;
	const uint16_t bufferHeight = 2048;
	for(int frame = 0; frame < sizeof(_frame_builders) / sizeof(*_frame_builders); frame++)
	{
		va_list va;
		va_copy(va, sizes);
		_frame_builders[frame] = new CRTFrameBuilder(bufferWidth, bufferHeight, number, va);
		va_end(va);
	}
	_current_frame_builder = _frame_builders[0];
}

CRT::CRT() :
	_next_scan(0),
	_frame_read_pointer(0),
	_sync_capacitor_charge_level(0),
	_is_receiving_sync(false),
	_current_frame_mutex(new std::mutex),
	_visible_area(Rect(0, 0, 1, 1)),
	_rasterPosition({.x = 0, .y = 0})
{
	construct_openGL();
}

CRT::CRT(unsigned int cycles_per_line, unsigned int height_of_display, ColourSpace colour_space, unsigned int colour_cycle_numerator, unsigned int colour_cycle_denominator, unsigned int number_of_buffers, ...) : CRT()
{
	set_new_timing(cycles_per_line, height_of_display, colour_space, colour_cycle_numerator, colour_cycle_denominator);

	va_list buffer_sizes;
	va_start(buffer_sizes, number_of_buffers);
	allocate_buffers(number_of_buffers, buffer_sizes);
	va_end(buffer_sizes);
}

CRT::CRT(unsigned int cycles_per_line, DisplayType displayType, unsigned int number_of_buffers, ...) : CRT()
{
	set_new_display_type(cycles_per_line, displayType);

	va_list buffer_sizes;
	va_start(buffer_sizes, number_of_buffers);
	allocate_buffers(number_of_buffers, buffer_sizes);
	va_end(buffer_sizes);
}

CRT::~CRT()
{
	for(int frame = 0; frame < sizeof(_frame_builders) / sizeof(*_frame_builders); frame++)
	{
		delete _frame_builders[frame];
	}
	destruct_openGL();
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

void CRT::advance_cycles(unsigned int number_of_cycles, unsigned int source_divider, bool hsync_requested, bool vsync_requested, const bool vsync_charging, const Type type, uint16_t tex_x, uint16_t tex_y)
{
	number_of_cycles *= _time_multiplier;

	bool is_output_run = ((type == Type::Level) || (type == Type::Data));
	vsync_requested &= (_sync_capacitor_charge_level >= _sync_capacitor_charge_threshold);

	while(number_of_cycles) {

		unsigned int time_until_vertical_sync_event, time_until_horizontal_sync_event;
		Flywheel::SyncEvent next_vertical_sync_event = get_next_vertical_sync_event(vsync_requested, number_of_cycles, &time_until_vertical_sync_event);
		Flywheel::SyncEvent next_horizontal_sync_event = get_next_horizontal_sync_event(hsync_requested, time_until_vertical_sync_event, &time_until_horizontal_sync_event);

		// get the next sync event and its timing; hsync request is instantaneous (being edge triggered) so
		// set it to false for the next run through this loop (if any)
		unsigned int next_run_length = std::min(time_until_vertical_sync_event, time_until_horizontal_sync_event);

		hsync_requested = false;
		vsync_requested = false;

		uint8_t *next_run = (is_output_run && _current_frame_builder && next_run_length) ? _current_frame_builder->get_next_run() : nullptr;
		int lengthMask = (_horizontal_flywheel->is_in_retrace() ? kRetraceXMask : 0) | (_vertical_flywheel->is_in_retrace() ? kRetraceYMask : 0);

#define position_x(v)	(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfPosition + 0])
#define position_y(v)	(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfPosition + 2])
#define tex_x(v)		(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfTexCoord + 0])
#define tex_y(v)		(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfTexCoord + 2])
#define lateral(v)		next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfLateral]

#define InternalToUInt16(v) ((v) + 32768) >> 16

		if(next_run)
		{
			unsigned int x_position = _horizontal_flywheel->get_current_output_position() * (kCRTFixedPointRange / 1024);
			unsigned int y_position = (_vertical_flywheel->get_current_output_position() / 312) * (kCRTFixedPointRange / 1024);

			// set the type, initial raster position and type of this run
			position_x(0) = position_x(4) = InternalToUInt16(kCRTFixedPointOffset + x_position + _beamWidth[lengthMask].x);
			position_y(0) = position_y(4) = InternalToUInt16(kCRTFixedPointOffset + y_position + _beamWidth[lengthMask].y);
			position_x(1) = InternalToUInt16(kCRTFixedPointOffset + x_position - _beamWidth[lengthMask].x);
			position_y(1) = InternalToUInt16(kCRTFixedPointOffset + y_position - _beamWidth[lengthMask].y);

			tex_x(0) = tex_x(1) = tex_x(4) = tex_x;

			// these things are constants across the line so just throw them out now
			tex_y(0) = tex_y(4) = tex_y(1) = tex_y(2) = tex_y(3) = tex_y(5) = tex_y;
			lateral(0) = lateral(4) = lateral(5) = 0;
			lateral(1) = lateral(2) = lateral(3) = 1;
		}

		// decrement the number of cycles left to run for and increment the
		// horizontal counter appropriately
		number_of_cycles -= next_run_length;

		// either charge or deplete the vertical retrace capacitor (making sure it stops at 0)
		if (vsync_charging && !_vertical_flywheel->is_in_retrace())
			_sync_capacitor_charge_level += next_run_length;
		else
			_sync_capacitor_charge_level = std::max(_sync_capacitor_charge_level - (int)next_run_length, 0);

		// react to the incoming event...
		_horizontal_flywheel->apply_event(next_run_length, (next_run_length == time_until_horizontal_sync_event) ? next_horizontal_sync_event : Flywheel::SyncEvent::None);
		_vertical_flywheel->apply_event(next_run_length, (next_run_length == time_until_vertical_sync_event) ? next_vertical_sync_event : Flywheel::SyncEvent::None);

		if(next_run)
		{
			unsigned int x_position = _horizontal_flywheel->get_current_output_position() * (kCRTFixedPointRange / 1024);
			unsigned int y_position = (_vertical_flywheel->get_current_output_position() / 312) * (kCRTFixedPointRange / 1024);

			// store the final raster position
			position_x(2) = position_x(3) = InternalToUInt16(kCRTFixedPointOffset + x_position - _beamWidth[lengthMask].x);
			position_y(2) = position_y(3) = InternalToUInt16(kCRTFixedPointOffset + y_position - _beamWidth[lengthMask].y);
			position_x(5) = InternalToUInt16(kCRTFixedPointOffset + x_position + _beamWidth[lengthMask].x);
			position_y(5) = InternalToUInt16(kCRTFixedPointOffset + y_position + _beamWidth[lengthMask].y);

			// if this is a data run then advance the buffer pointer
			if(type == Type::Data && source_divider) tex_x += next_run_length / (_time_multiplier * source_divider);

			// if this is a data or level run then store the end point
			tex_x(2) = tex_x(3) = tex_x(5) = tex_x;
		}

		if(next_run_length == time_until_vertical_sync_event && next_vertical_sync_event == Flywheel::SyncEvent::EndRetrace)
		{
			if(_current_frame_builder)
			{
				_current_frame_builder->complete();
				_current_frame_mutex->lock();
				_current_frame = &_current_frame_builder->frame;
				_current_frame_mutex->unlock();
				// TODO: how to communicate did_detect_vsync? Bring the delegate back?
//				_delegate->crt_did_end_frame(this, &_current_frame_builder->frame, _did_detect_vsync);
			}

//			if(_frames_with_delegate < kCRTNumberOfFrames)
//			{
			_frame_read_pointer = (_frame_read_pointer + 1)%kCRTNumberOfFrames;
			_current_frame_builder = _frame_builders[_frame_read_pointer];
			_current_frame_builder->reset();
//			}
//			else
//				_current_frame_builder = nullptr;

		}
	}
}

#pragma mark - stream feeding methods

void CRT::output_scan()
{
	_next_scan ^= 1;
	Scan *scan = &_scans[_next_scan];

	bool this_is_sync = (scan->type == Type::Sync);
	bool hsync_requested = !_is_receiving_sync && this_is_sync;
	bool vsync_requested = _is_receiving_sync && !this_is_sync;
	_is_receiving_sync = this_is_sync;

	advance_cycles(scan->number_of_cycles, scan->source_divider, hsync_requested, vsync_requested, this_is_sync, scan->type, scan->tex_x, scan->tex_y);
}

/*
	These all merely channel into advance_cycles, supplying appropriate arguments
*/
void CRT::output_sync(unsigned int number_of_cycles)
{
	_scans[_next_scan].type = Type::Sync;
	_scans[_next_scan].number_of_cycles = number_of_cycles;
	output_scan();
}

void CRT::output_blank(unsigned int number_of_cycles)
{
	_scans[_next_scan].type = Type::Blank;
	_scans[_next_scan].number_of_cycles = number_of_cycles;
	output_scan();
}

void CRT::output_level(unsigned int number_of_cycles)
{
	_scans[_next_scan].type = Type::Level;
	_scans[_next_scan].number_of_cycles = number_of_cycles;
	_scans[_next_scan].tex_x = _current_frame_builder ? _current_frame_builder->_write_x_position : 0;
	_scans[_next_scan].tex_y = _current_frame_builder ? _current_frame_builder->_write_y_position : 0;
	output_scan();
}

void CRT::output_colour_burst(unsigned int number_of_cycles, uint8_t phase, uint8_t magnitude)
{
	_scans[_next_scan].type = Type::ColourBurst;
	_scans[_next_scan].number_of_cycles = number_of_cycles;
	_scans[_next_scan].phase = phase;
	_scans[_next_scan].magnitude = magnitude;
	output_scan();
}

void CRT::output_data(unsigned int number_of_cycles, unsigned int source_divider)
{
	if(_current_frame_builder) _current_frame_builder->reduce_previous_allocation_to(number_of_cycles / source_divider);

	_scans[_next_scan].type = Type::Data;
	_scans[_next_scan].number_of_cycles = number_of_cycles;
	_scans[_next_scan].tex_x = _current_frame_builder ? _current_frame_builder->_write_x_position : 0;
	_scans[_next_scan].tex_y = _current_frame_builder ? _current_frame_builder->_write_y_position : 0;
	_scans[_next_scan].source_divider = source_divider;
	output_scan();
}

#pragma mark - Buffer supply

void CRT::allocate_write_area(int required_length)
{
	if(_current_frame_builder) _current_frame_builder->allocate_write_area(required_length);
}

uint8_t *CRT::get_write_target_for_buffer(int buffer)
{
	if (!_current_frame_builder) return nullptr;
	return _current_frame_builder->get_write_target_for_buffer(buffer);
}
