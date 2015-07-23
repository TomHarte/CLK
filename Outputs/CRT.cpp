//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include <stdarg.h>


using namespace Outputs;

CRT::CRT(int cycles_per_line, int height_of_display, int number_of_buffers, ...)
{
	const int syncCapacityLineChargeThreshold = 3;
	const int millisecondsHorizontalRetraceTime = 7;	// source: Dictionary of Video and Television Technology, p. 234
	const int scanlinesVerticalRetraceTime = 10;		// source: ibid

	// store fundamental display configuration properties
	_height_of_display = height_of_display;
	_cycles_per_line = cycles_per_line;

	// generate timing values implied by the given arbuments
	_hsync_error_window = cycles_per_line >> 5;

	_sync_capacitor_charge_threshold = syncCapacityLineChargeThreshold * cycles_per_line;
	_horizontal_retrace_time = (millisecondsHorizontalRetraceTime * cycles_per_line) >> 6;
	_vertical_retrace_time = scanlinesVerticalRetraceTime * cycles_per_line;

	_scanSpeed.x = 1.0f / (float)cycles_per_line;
	_scanSpeed.y = 1.0f / (float)(height_of_display * cycles_per_line);
	_retraceSpeed.x = 1.0f / (float)_horizontal_retrace_time;
	_retraceSpeed.y = 1.0f / (float)_vertical_retrace_time;

	// generate buffers for signal storage as requested — format is
	// number of buffers, size of buffer 1, size of buffer 2...
	const int bufferWidth = 512;
	const int bufferHeight = 512;
	for(int frame = 0; frame < 3; frame++)
	{
		va_list va;
		va_start(va, number_of_buffers);
		_frames[frame] = new CRTFrame(bufferWidth, bufferHeight, number_of_buffers, va);
		va_end(va);
	}
	_frames_with_delegate = 0;
	_frame_read_pointer = 0;
	_current_frame = _frames[0];

	// reset raster position
	_rasterPosition.x = _rasterPosition.y = 0.0f;

	// reset flywheel sync
	_expected_next_hsync = cycles_per_line;
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
		delete _frames[frame];
	}
}

#pragma mark - Sync loop

CRT::SyncEvent CRT::next_vertical_sync_event(bool vsync_is_charging, int cycles_to_run_for, int *cycles_advanced)
{
	SyncEvent proposedEvent = SyncEvent::None;
	int proposedSyncTime = cycles_to_run_for;

	// have we overrun the maximum permitted number of horizontal syncs for this frame?
	if (!_vretrace_counter)
	{
		float raster_distance = _scanSpeed.y * (float)proposedSyncTime;
		if(_rasterPosition.y < 1.02f && _rasterPosition.y + raster_distance >= 1.02f) {
			proposedSyncTime = (int)(1.02f - _rasterPosition.y) / _scanSpeed.y;
			proposedEvent = SyncEvent::StartVSync;
		}
	}

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

void CRT::advance_cycles(int number_of_cycles, bool hsync_requested, const bool vsync_charging, const CRTRun::Type type, const char *data_type)
{
	// this is safe to keep locally because it accumulates over this run of cycles only
	int buffer_offset = 0;

	while(number_of_cycles) {

		int time_until_vertical_sync_event, time_until_horizontal_sync_event;
		SyncEvent next_vertical_sync_event = this->next_vertical_sync_event(vsync_charging, number_of_cycles, &time_until_vertical_sync_event);
		SyncEvent next_horizontal_sync_event = this->next_horizontal_sync_event(hsync_requested, time_until_vertical_sync_event, &time_until_horizontal_sync_event);
		hsync_requested = false;

		// get the next sync event and its timing; hsync request is instantaneous (being edge triggered) so
		// set it to false for the next run through this loop (if any)
		int next_run_length = std::min(time_until_vertical_sync_event, time_until_horizontal_sync_event);

		CRTRun *nextRun = (_current_frame && next_run_length) ? _current_frame->get_next_run() : nullptr;

		if(nextRun)
		{
			// set the type, initial raster position and type of this run
			nextRun->type = type;
			nextRun->start_point.dst_x = _rasterPosition.x;
			nextRun->start_point.dst_y = _rasterPosition.y;
			nextRun->data_type = data_type;

			// if this is a data or level run then store a starting data position
			if(type == CRTRun::Type::Data || type == CRTRun::Type::Level)
			{
				nextRun->start_point.src_x = (_current_frame->_write_target_pointer + buffer_offset) & (_current_frame->size.width - 1);
				nextRun->start_point.src_y = (_current_frame->_write_target_pointer + buffer_offset) / _current_frame->size.width;
			}

			// advance the raster position as dictated by current sync status
			if (_is_in_hsync)
				_rasterPosition.x = std::max(0.0f, _rasterPosition.x - (float)number_of_cycles * _retraceSpeed.x);
			else
				_rasterPosition.x = std::min(1.0f, _rasterPosition.x + (float)number_of_cycles * _scanSpeed.x);

			if (_vretrace_counter > 0)
				_rasterPosition.y = std::max(0.0f, _rasterPosition.y - (float)number_of_cycles * _retraceSpeed.y);
			else
				_rasterPosition.y = std::min(1.0f, _rasterPosition.y + (float)number_of_cycles * _scanSpeed.y);

			// store the final raster position
			nextRun->end_point.dst_x = _rasterPosition.x;
			nextRun->end_point.dst_y = _rasterPosition.y;

			// if this is a data run then advance the buffer pointer
			if(type == CRTRun::Type::Data)
			{
				buffer_offset += next_run_length;
			}

			// if this is a data or level run then store the end point
			if(type == CRTRun::Type::Data || type == CRTRun::Type::Level)
			{
				nextRun->end_point.src_x = (_current_frame->_write_target_pointer + buffer_offset) & (_current_frame->size.width - 1);
				nextRun->end_point.src_y = (_current_frame->_write_target_pointer + buffer_offset) / _current_frame->size.width;
			}
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
					if(_delegate && _current_frame)
					{
						_current_frame->complete();
						_frames_with_delegate++;
						_delegate->crt_did_end_frame(this, _current_frame);
					}

					if(_frames_with_delegate < kCRTNumberOfFrames)
					{
						_frame_read_pointer = (_frame_read_pointer + 1)%kCRTNumberOfFrames;
						_current_frame = _frames[_frame_read_pointer];
						_current_frame->reset();
					}
					else
						_current_frame = nullptr;
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
	advance_cycles(number_of_cycles, _hsync_requested, true, CRTRun::Type::Sync, nullptr);
}

void CRT::output_blank(int number_of_cycles)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Blank, nullptr);
}

void CRT::output_level(int number_of_cycles, const char *type)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Level, type);
}

void CRT::output_data(int number_of_cycles, const char *type)
{
	_is_receiving_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Data, type);
}

#pragma mark - Buffer supply

void CRT::allocate_write_area(int required_length)
{
	if(_current_frame) _current_frame->allocate_write_area(required_length);
}

uint8_t *CRT::get_write_target_for_buffer(int buffer)
{
	if (!_current_frame) return nullptr;
	return _current_frame->get_write_target_for_buffer(buffer);
}

#pragma mark - CRTFrame

CRTFrame::CRTFrame(int width, int height, int number_of_buffers, va_list buffer_sizes)
{
	size.width = width;
	size.height = height;
	this->number_of_buffers = number_of_buffers;
	buffers = new CRTBuffer[number_of_buffers];

	for(int buffer = 0; buffer < number_of_buffers; buffer++)
	{
		buffers[buffer].depth = va_arg(buffer_sizes, int);
		buffers[buffer].data = new uint8_t[width * height * buffers[buffer].depth];
	}

	reset();
}

CRTFrame::~CRTFrame()
{
	for(int buffer = 0; buffer < number_of_buffers; buffer++)
		delete[] buffers[buffer].data;
	delete buffers;
}

void CRTFrame::reset()
{
	number_of_runs = 0;
	_write_allocation_pointer = _write_target_pointer = 0;
}

void CRTFrame::complete()
{
	runs = &_all_runs[0];
}

CRTRun *CRTFrame::get_next_run()
{
	// get a run from the allocated list, allocating more if we're about to overrun
	if(number_of_runs >= _all_runs.size())
	{
		_all_runs.resize((_all_runs.size() * 2)+1);
	}

	CRTRun *nextRun = &_all_runs[number_of_runs];
	number_of_runs++;

	return nextRun;
}

void CRTFrame::allocate_write_area(int required_length)
{
	int xPos = _write_allocation_pointer & (size.width - 1);
	if (xPos + required_length > size.width)
	{
		_write_allocation_pointer &= ~(size.width - 1);
		_write_allocation_pointer = (_write_allocation_pointer + size.width) & ((size.height-1) * size.width);
	}

	_write_target_pointer = _write_allocation_pointer;
	_write_allocation_pointer += required_length;
}

uint8_t *CRTFrame::get_write_target_for_buffer(int buffer)
{
	return &buffers[buffer].data[_write_target_pointer * buffers[buffer].depth];
}
