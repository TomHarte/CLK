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

#define kRetraceXMask	0x01
#define kRetraceYMask	0x02

CRT::CRT(int cycles_per_line, int height_of_display, int number_of_buffers, ...)
{
	const int syncCapacityLineChargeThreshold = 5;
	const int millisecondsHorizontalRetraceTime = 7;	// source: Dictionary of Video and Television Technology, p. 234
	const int scanlinesVerticalRetraceTime = 10;		// source: ibid

	_time_multiplier = (1000 + cycles_per_line - 1) / cycles_per_line;

	// store fundamental display configuration properties
	_height_of_display = height_of_display;// + (height_of_display / 10);
	_cycles_per_line = cycles_per_line * _time_multiplier;

	// generate timing values implied by the given arbuments
	_hsync_error_window = _cycles_per_line >> 5;

	_sync_capacitor_charge_threshold = (syncCapacityLineChargeThreshold * _cycles_per_line) >> 1;
	_horizontal_retrace_time = (millisecondsHorizontalRetraceTime * _cycles_per_line) >> 6;
	_vertical_retrace_time = scanlinesVerticalRetraceTime * _cycles_per_line;

	_scanSpeed.x = UINT32_MAX / _cycles_per_line;
	_scanSpeed.y = UINT32_MAX / (_height_of_display * _cycles_per_line);
	_retraceSpeed.x = UINT32_MAX / _horizontal_retrace_time;
	_retraceSpeed.y = UINT32_MAX / _vertical_retrace_time;

	// precompute the lengths of all four combinations of scan direction, for fast triangle
	// strip generation later
	float scanSpeedXfl = 1.0f / (float)_cycles_per_line;
	float scanSpeedYfl = 1.0f / (float)(_height_of_display * _cycles_per_line);
	float retraceSpeedXfl = 1.0f / (float)_horizontal_retrace_time;
	float retraceSpeedYfl = 1.0f / (float)(_vertical_retrace_time);
	float lengths[4];

	lengths[0]								= sqrtf(scanSpeedXfl*scanSpeedXfl		+ scanSpeedYfl*scanSpeedYfl);
	lengths[kRetraceXMask]					= sqrtf(retraceSpeedXfl*retraceSpeedXfl + scanSpeedYfl*scanSpeedYfl);
	lengths[kRetraceXMask | kRetraceYMask]	= sqrtf(retraceSpeedXfl*retraceSpeedXfl + retraceSpeedYfl*retraceSpeedYfl);
	lengths[kRetraceYMask]					= sqrtf(scanSpeedXfl*scanSpeedXfl		+ retraceSpeedYfl*retraceSpeedYfl);

	// width should be 1.0 / _height_of_display, rotated to match the direction
	float angle = atan2f(scanSpeedYfl, scanSpeedXfl);
	float halfLineWidth = (float)_height_of_display * 1.9f;
	_widths[0][0] = (sinf(angle) / halfLineWidth) * UINT32_MAX;
	_widths[0][1] = (cosf(angle) / halfLineWidth) * UINT32_MAX;

	// generate buffers for signal storage as requested — format is
	// number of buffers, size of buffer 1, size of buffer 2...
	const int bufferWidth = 512;
	const int bufferHeight = 512;
	for(int frame = 0; frame < 3; frame++)
	{
		va_list va;
		va_start(va, number_of_buffers);
		_frame_builders[frame] = new CRTFrameBuilder(bufferWidth, bufferHeight, number_of_buffers, va);
		va_end(va);
	}
	_frames_with_delegate = 0;
	_frame_read_pointer = 0;
	_current_frame_builder = _frame_builders[0];

	// reset raster position
	_rasterPosition.x = _rasterPosition.y = 0;

	// reset flywheel sync
	_expected_next_hsync = _cycles_per_line;
	_horizontal_counter = 0;

	// reset the vertical charge capacitor
	_sync_capacitor_charge_level = 0;

	// start off not in horizontal sync, not receiving a sync signal
	_is_receiving_sync = false;
	_is_in_hsync = false;
	_vretrace_counter = 0;
}

CRT::~CRT()
{
	for(int frame = 0; frame < 3; frame++)
	{
		delete _frame_builders[frame];
	}
}

#pragma mark - Sync loop

CRT::SyncEvent CRT::next_vertical_sync_event(bool vsync_is_charging, int cycles_to_run_for, int *cycles_advanced)
{
	SyncEvent proposedEvent = SyncEvent::None;
	int proposedSyncTime = cycles_to_run_for;

	// have we overrun the maximum permitted number of horizontal syncs for this frame?
//	if (!_vretrace_counter)
//	{
//		float raster_distance = _scanSpeed.y * proposedSyncTime;
//		if(_rasterPosition.y < 1.02f && _rasterPosition.y + raster_distance >= 1.02f) {
//			proposedSyncTime = (int)(1.02f - _rasterPosition.y) / _scanSpeed.y;
//			proposedEvent = SyncEvent::StartVSync;
//		}
//	}

	// will an acceptable vertical sync be triggered?
	if (vsync_is_charging && !_vretrace_counter) {
		 if (_sync_capacitor_charge_level < _sync_capacitor_charge_threshold && _sync_capacitor_charge_level + proposedSyncTime >= _sync_capacitor_charge_threshold) {
			proposedSyncTime = _sync_capacitor_charge_threshold - _sync_capacitor_charge_level;
			proposedEvent = SyncEvent::StartVSync;
		 }
	}

	// will an ongoing vertical sync end?
	if (_vretrace_counter > 0) {
		if (_vretrace_counter < proposedSyncTime) {
			proposedSyncTime = _vretrace_counter;
			proposedEvent = SyncEvent::EndVSync;
		}
	}

	*cycles_advanced = proposedSyncTime;
	return proposedEvent;
}

CRT::SyncEvent CRT::next_horizontal_sync_event(bool hsync_is_requested, int cycles_to_run_for, int *cycles_advanced)
{
	// do we recognise this hsync, thereby adjusting future time expectations?
	if(hsync_is_requested) {
		if (_horizontal_counter < _hsync_error_window || _horizontal_counter >= _expected_next_hsync - _hsync_error_window) {
			_did_detect_hsync = true;

			int time_now = (_horizontal_counter < _hsync_error_window) ? _expected_next_hsync + _horizontal_counter : _horizontal_counter;
			_expected_next_hsync = (_expected_next_hsync + time_now) >> 1;
		}
	}

	SyncEvent proposedEvent = SyncEvent::None;
	int proposedSyncTime = cycles_to_run_for;

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

void CRT::advance_cycles(int number_of_cycles, bool hsync_requested, const bool vsync_charging, const Type type, const char *data_type)
{

	number_of_cycles *= _time_multiplier;

	bool is_output_run = ((type == Type::Level) || (type == Type::Data));
	uint16_t tex_x = 0;
	uint16_t tex_y = 0;

	if(is_output_run && _current_frame_builder) {
		tex_x = _current_frame_builder->_write_x_position;
		tex_y = _current_frame_builder->_write_y_position;
	}

	while(number_of_cycles) {

		int time_until_vertical_sync_event, time_until_horizontal_sync_event;
		SyncEvent next_vertical_sync_event = this->next_vertical_sync_event(vsync_charging, number_of_cycles, &time_until_vertical_sync_event);
		SyncEvent next_horizontal_sync_event = this->next_horizontal_sync_event(hsync_requested, time_until_vertical_sync_event, &time_until_horizontal_sync_event);
		hsync_requested = false;

		// get the next sync event and its timing; hsync request is instantaneous (being edge triggered) so
		// set it to false for the next run through this loop (if any)
		int next_run_length = std::min(time_until_vertical_sync_event, time_until_horizontal_sync_event);

		uint16_t *next_run = (is_output_run && _current_frame_builder && next_run_length) ? _current_frame_builder->get_next_run() : nullptr;
//		int lengthMask = (_is_in_hsync ? kRetraceXMask : 0) | ((_vretrace_counter > 0) ? kRetraceXMask : 0);
//		uint32_t *width = _widths[lengthMask];
		uint32_t *width = _widths[0];

		if(next_run)
		{
			// set the type, initial raster position and type of this run
			next_run[0] = next_run[20] = (_rasterPosition.x + width[0]) >> 16;
			next_run[1] = next_run[21] = (_rasterPosition.y + width[1]) >> 16;
			next_run[4] = (_rasterPosition.x - width[0]) >> 16;
			next_run[5] = (_rasterPosition.y - width[1]) >> 16;

			next_run[2] = next_run[6] = next_run[22] = tex_x;
			next_run[3] = next_run[7] = next_run[23] = tex_y;
		}

		// advance the raster position as dictated by current sync status
		if (_is_in_hsync)
			_rasterPosition.x = (uint32_t)std::max((int64_t)0, (int64_t)_rasterPosition.x - number_of_cycles * (int64_t)_retraceSpeed.x);
		else
			_rasterPosition.x = (uint32_t)std::min((int64_t)UINT32_MAX, (int64_t)_rasterPosition.x + number_of_cycles * (int64_t)_scanSpeed.x);

		if (_vretrace_counter > 0)
			_rasterPosition.y = (uint32_t)std::max((int64_t)0, (int64_t)_rasterPosition.y - number_of_cycles * (int64_t)_retraceSpeed.y);
		else
			_rasterPosition.y = (uint32_t)std::min((int64_t)UINT32_MAX, (int64_t)_rasterPosition.y + number_of_cycles * (int64_t)_scanSpeed.y);

		if(next_run)
		{
			// store the final raster position
			next_run[8] = (_rasterPosition.x - width[0]) >> 16;
			next_run[9] = (_rasterPosition.y - width[1]) >> 16;
			next_run[12] = (_rasterPosition.x - width[0]) >> 16;
			next_run[13] = (_rasterPosition.y - width[1]) >> 16;

			next_run[16] = (_rasterPosition.x + width[0]) >> 16;
			next_run[17] = (_rasterPosition.y + width[1]) >> 16;

			// if this is a data run then advance the buffer pointer
			if(type == Type::Data) tex_x += next_run_length / _time_multiplier;

			// if this is a data or level run then store the end point
			next_run[10] = next_run[14] = next_run[18] = tex_x;
			next_run[11] = next_run[15] = next_run[19] = tex_y;
		}

		// decrement the number of cycles left to run for and increment the
		// horizontal counter appropriately
		number_of_cycles -= next_run_length;
		_horizontal_counter += next_run_length;

		// either charge or deplete the vertical retrace capacitor (making sure it stops at 0)
		if (vsync_charging)
			_sync_capacitor_charge_level += next_run_length;
		else
			_sync_capacitor_charge_level = std::max(_sync_capacitor_charge_level - next_run_length, 0);

		// decrement the vertical retrace counter, making sure it stops at 0
		_vretrace_counter = std::max(_vretrace_counter - next_run_length, 0);

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
					_vretrace_counter = _vertical_retrace_time;
				break;

				// end of vertical sync: tell the delegate that we finished vertical sync,
				// releasing all runs back into the common pool
				case SyncEvent::EndVSync:
					if(_delegate && _current_frame_builder)
					{
						_current_frame_builder->complete();
						_frames_with_delegate++;
						_delegate->crt_did_end_frame(this, &_current_frame_builder->frame);
					}

					if(_frames_with_delegate < kCRTNumberOfFrames)
					{
						_frame_read_pointer = (_frame_read_pointer + 1)%kCRTNumberOfFrames;
						_current_frame_builder = _frame_builders[_frame_read_pointer];
						_current_frame_builder->reset();
					}
					else
						_current_frame_builder = nullptr;
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

void CRT::set_delegate(CRTDelegate *delegate)
{
	_delegate = delegate;
}

#pragma mark - stream feeding methods

/*
	These all merely channel into advance_cycles, supplying appropriate arguments
*/
void CRT::output_sync(int number_of_cycles)
{
	bool _hsync_requested = !_is_receiving_sync;	// ensure this really is edge triggered; someone calling output_sync twice in succession shouldn't trigger it twice
	_is_receiving_sync = true;
	advance_cycles(number_of_cycles, _hsync_requested, true, Type::Sync, nullptr);
}

void CRT::output_blank(int number_of_cycles)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, Type::Blank, nullptr);
}

void CRT::output_level(int number_of_cycles, const char *type)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, Type::Level, type);
}

void CRT::output_data(int number_of_cycles, const char *type)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, Type::Data, type);
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

CRTFrameBuilder::CRTFrameBuilder(int width, int height, int number_of_buffers, va_list buffer_sizes)
{
	frame.size.width = width;
	frame.size.height = height;
	frame.number_of_buffers = number_of_buffers;
	frame.buffers = new CRTBuffer[number_of_buffers];

	for(int buffer = 0; buffer < number_of_buffers; buffer++)
	{
		frame.buffers[buffer].depth = va_arg(buffer_sizes, int);
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
	frame.number_of_runs = 0;
	_next_write_x_position = _next_write_y_position = 0;
	frame.dirty_size.width = frame.dirty_size.height = 0;
}

void CRTFrameBuilder::complete()
{
	frame.runs = &_all_runs[0];
}

uint16_t *CRTFrameBuilder::get_next_run()
{
	// get a run from the allocated list, allocating more if we're about to overrun
	if(frame.number_of_runs * 24 >= _all_runs.size())
	{
		_all_runs.resize(_all_runs.size() + 2400);
	}

	uint16_t *next_run = &_all_runs[frame.number_of_runs * 24];
	frame.number_of_runs++;

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
