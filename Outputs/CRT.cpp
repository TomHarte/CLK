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

void CRT::set_new_timing(unsigned int cycles_per_line, unsigned int height_of_display)
{
	const unsigned int syncCapacityLineChargeThreshold = 3;
	const unsigned int millisecondsHorizontalRetraceTime = 7;	// source: Dictionary of Video and Television Technology, p. 234
	const unsigned int scanlinesVerticalRetraceTime = 10;		// source: ibid

																// To quote:
																//
																//	"retrace interval; The interval of time for the return of the blanked scanning beam of
																//	a TV picture tube or camera tube to the starting point of a line or field. It is about 7 µs
																//	for horizontal retrace and 500 to 750 µs for vertical retrace in NTSC and PAL TV."

	_time_multiplier = (1000 + cycles_per_line - 1) / cycles_per_line;
	height_of_display += (height_of_display / 20);  // this is the overrun area we'll use to

	// store fundamental display configuration properties
	_height_of_display = height_of_display + 5;
	_cycles_per_line = cycles_per_line * _time_multiplier;

	// generate timing values implied by the given arbuments
	_hsync_error_window = _cycles_per_line >> 5;

	_sync_capacitor_charge_threshold = ((syncCapacityLineChargeThreshold * _cycles_per_line) * 50) >> 7;
	_horizontal_retrace_time = (millisecondsHorizontalRetraceTime * _cycles_per_line) >> 6;
	const unsigned int vertical_retrace_time = scanlinesVerticalRetraceTime * _cycles_per_line;
	const float halfLineWidth = (float)_height_of_display * 2.0f;

	for(int c = 0; c < 4; c++)
	{
		_scanSpeed[c].x = (c&kRetraceXMask) ? -(kCRTFixedPointRange / _horizontal_retrace_time)	: (kCRTFixedPointRange / _cycles_per_line);
		_scanSpeed[c].y = (c&kRetraceYMask) ? -(kCRTFixedPointRange / vertical_retrace_time)	: (kCRTFixedPointRange / (_height_of_display * _cycles_per_line));

		// width should be 1.0 / _height_of_display, rotated to match the direction
		float angle = atan2f(_scanSpeed[c].y, _scanSpeed[c].x);
		_beamWidth[c].x = (uint32_t)((sinf(angle) / halfLineWidth) * kCRTFixedPointRange);
		_beamWidth[c].y = (uint32_t)((cosf(angle) / halfLineWidth) * kCRTFixedPointRange);
	}
}

CRT::CRT(unsigned int cycles_per_line, unsigned int height_of_display, unsigned int number_of_buffers, ...) :
	_next_scan(0),
	_frames_with_delegate(0),
	_frame_read_pointer(0),
	_horizontal_counter(0),
	_sync_capacitor_charge_level(0),
	_is_receiving_sync(false),
	_is_in_hsync(false),
	_is_in_vsync(false),
	_rasterPosition({.x = 0, .y = 0})
{
	set_new_timing(cycles_per_line, height_of_display);

	// generate buffers for signal storage as requested — format is
	// number of buffers, size of buffer 1, size of buffer 2...
	const uint16_t bufferWidth = 2048;
	const uint16_t bufferHeight = 2048;
	for(int frame = 0; frame < sizeof(_frame_builders) / sizeof(*_frame_builders); frame++)
	{
		va_list va;
		va_start(va, number_of_buffers);
		_frame_builders[frame] = new CRTFrameBuilder(bufferWidth, bufferHeight, number_of_buffers, va);
		va_end(va);
	}
	_current_frame_builder = _frame_builders[0];

	// reset flywheel sync
	_expected_next_hsync = _cycles_per_line;
}

CRT::~CRT()
{
	for(int frame = 0; frame < sizeof(_frame_builders) / sizeof(*_frame_builders); frame++)
	{
		delete _frame_builders[frame];
	}
}

#pragma mark - Sync loop

CRT::SyncEvent CRT::get_next_vertical_sync_event(bool vsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced)
{
	SyncEvent proposedEvent = SyncEvent::None;
	unsigned int proposedSyncTime = cycles_to_run_for;

	// will an acceptable vertical sync be triggered?
	if (vsync_is_requested && !_is_in_vsync) {
		if (_sync_capacitor_charge_level >= _sync_capacitor_charge_threshold && _rasterPosition.y >= 3*(kCRTFixedPointRange >> 2)) {
			proposedSyncTime = 0;
			proposedEvent = SyncEvent::StartVSync;
			_did_detect_vsync = true;
		}
	}

	// have we overrun the maximum permitted number of horizontal syncs for this frame?
	if (!_is_in_vsync) {
		unsigned int time_until_end_of_frame = (kCRTFixedPointRange - _rasterPosition.y) / _scanSpeed[0].y;

		if(time_until_end_of_frame < proposedSyncTime) {
			proposedSyncTime = time_until_end_of_frame;
			proposedEvent = SyncEvent::StartVSync;
		}
	} else {
		unsigned int time_until_start_of_frame = _rasterPosition.y / (uint32_t)(-_scanSpeed[kRetraceYMask].y);

		if(time_until_start_of_frame < proposedSyncTime) {
			proposedSyncTime = time_until_start_of_frame;
			proposedEvent = SyncEvent::EndVSync;
		}
	}

	*cycles_advanced = proposedSyncTime;
	return proposedEvent;
}

CRT::SyncEvent CRT::get_next_horizontal_sync_event(bool hsync_is_requested, unsigned int cycles_to_run_for, unsigned int *cycles_advanced)
{
	// do we recognise this hsync, thereby adjusting future time expectations?
	if(hsync_is_requested) {
		if (_horizontal_counter < _hsync_error_window || _horizontal_counter >= _expected_next_hsync - _hsync_error_window) {
			_did_detect_hsync = true;

			unsigned int time_now = (_horizontal_counter < _hsync_error_window) ? _expected_next_hsync + _horizontal_counter : _horizontal_counter;
			_expected_next_hsync = (_expected_next_hsync + _expected_next_hsync + _expected_next_hsync + time_now) >> 2;
		}
	}

	SyncEvent proposedEvent = SyncEvent::None;
	unsigned int proposedSyncTime = cycles_to_run_for;

	// will we end an ongoing hsync?
	if (_horizontal_counter < _horizontal_retrace_time && _horizontal_counter+proposedSyncTime >= _horizontal_retrace_time) {
		proposedSyncTime = _horizontal_retrace_time - _horizontal_counter;
		proposedEvent = SyncEvent::EndHSync;
	}

	// will we start an hsync?
	if (_horizontal_counter + proposedSyncTime >= _expected_next_hsync) {
		proposedSyncTime = _expected_next_hsync - _horizontal_counter;
		proposedEvent = SyncEvent::StartHSync;
	}

	*cycles_advanced = proposedSyncTime;
	return proposedEvent;
}

void CRT::advance_cycles(unsigned int number_of_cycles, unsigned int source_divider, bool hsync_requested, bool vsync_requested, const bool vsync_charging, const Type type, uint16_t tex_x, uint16_t tex_y)
{
	number_of_cycles *= _time_multiplier;

	bool is_output_run = ((type == Type::Level) || (type == Type::Data));

	while(number_of_cycles) {

		unsigned int time_until_vertical_sync_event, time_until_horizontal_sync_event;
		SyncEvent next_vertical_sync_event = this->get_next_vertical_sync_event(vsync_requested, number_of_cycles, &time_until_vertical_sync_event);
		SyncEvent next_horizontal_sync_event = this->get_next_horizontal_sync_event(hsync_requested, time_until_vertical_sync_event, &time_until_horizontal_sync_event);

		// get the next sync event and its timing; hsync request is instantaneous (being edge triggered) so
		// set it to false for the next run through this loop (if any)
		unsigned int next_run_length = std::min(time_until_vertical_sync_event, time_until_horizontal_sync_event);

		hsync_requested = false;
		vsync_requested = false;

		uint8_t *next_run = (is_output_run && _current_frame_builder && next_run_length) ? _current_frame_builder->get_next_run() : nullptr;
		int lengthMask = (_is_in_hsync ? kRetraceXMask : 0) | (_is_in_vsync ? kRetraceYMask : 0);

#define position_x(v)	(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfPosition + 0])
#define position_y(v)	(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfPosition + 2])
#define tex_x(v)		(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfTexCoord + 0])
#define tex_y(v)		(*(uint16_t *)&next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfTexCoord + 2])
#define lateral(v)		next_run[kCRTSizeOfVertex*v + kCRTVertexOffsetOfLateral]

#define InternalToUInt16(v) ((v) + 32768) >> 16

		if(next_run)
		{
			// set the type, initial raster position and type of this run
			position_x(0) = position_x(4) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.x + _beamWidth[lengthMask].x);
			position_y(0) = position_y(4) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.y + _beamWidth[lengthMask].y);
			position_x(1) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.x - _beamWidth[lengthMask].x);
			position_y(1) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.y - _beamWidth[lengthMask].y);

			tex_x(0) = tex_x(1) = tex_x(4) = tex_x;

			// these things are constants across the line so just throw them out now
			tex_y(0) = tex_y(4) = tex_y(1) = tex_y(2) = tex_y(3) = tex_y(5) = tex_y;
			lateral(0) = lateral(4) = lateral(5) = 0;
			lateral(1) = lateral(2) = lateral(3) = 1;
		}

		// advance the raster position as dictated by current sync status
		int64_t end_position[2];
		end_position[0] = (int64_t)_rasterPosition.x + (int64_t)next_run_length * (int32_t)_scanSpeed[lengthMask].x;
		end_position[1] = (int64_t)_rasterPosition.y + (int64_t)next_run_length * (int32_t)_scanSpeed[lengthMask].y;

		if (_is_in_hsync)
			_rasterPosition.x = (uint32_t)std::max((int64_t)0, end_position[0]);
		else
			_rasterPosition.x = (uint32_t)std::min((int64_t)kCRTFixedPointRange, end_position[0]);

		if (_is_in_vsync)
			_rasterPosition.y = (uint32_t)std::max((int64_t)0, end_position[1]);
		else
			_rasterPosition.y = (uint32_t)std::min((int64_t)kCRTFixedPointRange, end_position[1]);

		if(next_run)
		{
			// store the final raster position
			position_x(2) = position_x(3) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.x - _beamWidth[lengthMask].x);
			position_y(2) = position_y(3) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.y - _beamWidth[lengthMask].y);
			position_x(5) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.x + _beamWidth[lengthMask].x);
			position_y(5) = InternalToUInt16(kCRTFixedPointOffset + _rasterPosition.y + _beamWidth[lengthMask].y);

			// if this is a data run then advance the buffer pointer
			if(type == Type::Data) tex_x += next_run_length / (_time_multiplier * source_divider);

			// if this is a data or level run then store the end point
			tex_x(2) = tex_x(3) = tex_x(5) = tex_x;
		}

		// decrement the number of cycles left to run for and increment the
		// horizontal counter appropriately
		number_of_cycles -= next_run_length;
		_horizontal_counter += next_run_length;

		// either charge or deplete the vertical retrace capacitor (making sure it stops at 0)
		if (vsync_charging && !_is_in_vsync)
			_sync_capacitor_charge_level += next_run_length;
		else
			_sync_capacitor_charge_level = std::max(_sync_capacitor_charge_level - (int)next_run_length, 0);

		// react to the incoming event...
		if(next_run_length == time_until_horizontal_sync_event)
		{
			switch(next_horizontal_sync_event)
			{
				// start of hsync: zero the scanline counter, note that we're now in
				// horizontal sync, increment the lines-in-this-frame counter
				case SyncEvent::StartHSync:
					_horizontal_counter = 0;
					_is_in_hsync = true;
				break;

				// end of horizontal sync: update the flywheel's velocity, note that we're no longer
				// in horizontal sync
				case SyncEvent::EndHSync:
					if (!_did_detect_hsync) {
						_expected_next_hsync = (_expected_next_hsync + (_hsync_error_window >> 1) + _cycles_per_line) >> 1;
					}
					_did_detect_hsync = false;
					_is_in_hsync = false;
				break;

				default: break;
			}
		}

		if(next_run_length == time_until_vertical_sync_event)
		{
			switch(next_vertical_sync_event)
			{
				// start of vertical sync: reset the lines-in-this-frame counter,
				// load the retrace counter with the amount of time it'll take to retrace
				case SyncEvent::StartVSync:
					_is_in_vsync = true;
					_sync_capacitor_charge_level = 0;
				break;

				// end of vertical sync: tell the delegate that we finished vertical sync,
				// releasing all runs back into the common pool
				case SyncEvent::EndVSync:
					if(_delegate && _current_frame_builder)
					{
						_current_frame_builder->complete();
						_frames_with_delegate++;
						_delegate->crt_did_end_frame(this, &_current_frame_builder->frame, _did_detect_vsync);
					}

					if(_frames_with_delegate < kCRTNumberOfFrames)
					{
						_frame_read_pointer = (_frame_read_pointer + 1)%kCRTNumberOfFrames;
						_current_frame_builder = _frame_builders[_frame_read_pointer];
						_current_frame_builder->reset();
					}
					else
						_current_frame_builder = nullptr;

					_is_in_vsync = false;
					_did_detect_vsync = false;
				break;

				default: break;
			}
		}
	}
}

void CRT::return_frame()
{
	_frames_with_delegate--;
}

#pragma mark - delegate

void CRT::set_delegate(Delegate *delegate)
{
	_delegate = delegate;
}

#pragma mark - stream feeding methods

void CRT::output_scan()
{
	_next_scan ^= 1;
	Scan *scan = &_scans[_next_scan];

	bool this_is_sync = (scan->type == Type::Sync);
	bool hsync_requested = !_is_receiving_sync && this_is_sync;
	bool vsync_requested = _is_receiving_sync;
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

#pragma mark - CRTFrame

CRTFrameBuilder::CRTFrameBuilder(uint16_t width, uint16_t height, unsigned int number_of_buffers, va_list buffer_sizes)
{
	frame.size.width = width;
	frame.size.height = height;
	frame.number_of_buffers = number_of_buffers;
	frame.buffers = new CRTBuffer[number_of_buffers];
	frame.size_per_vertex = kCRTSizeOfVertex;
	frame.geometry_mode = CRTGeometryModeTriangles;

	for(int buffer = 0; buffer < number_of_buffers; buffer++)
	{
		frame.buffers[buffer].depth = va_arg(buffer_sizes, unsigned int);
		frame.buffers[buffer].data = new uint8_t[width * height * frame.buffers[buffer].depth];
	}

	reset();
}

CRTFrameBuilder::~CRTFrameBuilder()
{
	for(int buffer = 0; buffer < frame.number_of_buffers; buffer++)
		delete[] frame.buffers[buffer].data;
	delete frame.buffers;
}

void CRTFrameBuilder::reset()
{
	frame.number_of_vertices = 0;
	_next_write_x_position = _next_write_y_position = 0;
	frame.dirty_size.width = 0;
	frame.dirty_size.height = 1;
}

void CRTFrameBuilder::complete()
{
	frame.vertices = &_all_runs[0];
}

uint8_t *CRTFrameBuilder::get_next_run()
{
	const size_t vertices_per_run = 6;

	// get a run from the allocated list, allocating more if we're about to overrun
	if((frame.number_of_vertices + vertices_per_run) * frame.size_per_vertex >= _all_runs.size())
	{
		_all_runs.resize(_all_runs.size() + frame.size_per_vertex * vertices_per_run * 100);
	}

	uint8_t *next_run = &_all_runs[frame.number_of_vertices * frame.size_per_vertex];
	frame.number_of_vertices += vertices_per_run;

	return next_run;
}

void CRTFrameBuilder::allocate_write_area(int required_length)
{
	if (_next_write_x_position + required_length > frame.size.width)
	{
		_next_write_x_position = 0;
		_next_write_y_position = (_next_write_y_position+1)&(frame.size.height-1);
		frame.dirty_size.height++;
	}

	_write_x_position = _next_write_x_position;
	_write_y_position = _next_write_y_position;
	_write_target_pointer = (_write_y_position * frame.size.width) + _write_x_position;
	_next_write_x_position += required_length;
	frame.dirty_size.width = std::max(frame.dirty_size.width, _next_write_x_position);
}

uint8_t *CRTFrameBuilder::get_write_target_for_buffer(int buffer)
{
	return &frame.buffers[buffer].data[_write_target_pointer * frame.buffers[buffer].depth];
}
