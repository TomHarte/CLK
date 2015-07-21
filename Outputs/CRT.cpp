//
//  CRT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "CRT.hpp"
#include <stdarg.h>

static const int bufferWidth = 512;
static const int bufferHeight = 512;

static const int syncCapacityLineChargeThreshold = 3;
static const int millisecondsHorizontalRetraceTime = 16;
static const int scanlinesVerticalRetraceTime = 26;

#define kEmergencyRetraceTime	(_expected_next_hsync + _hsync_error_window)

using namespace Outputs;

CRT::CRT(int cycles_per_line, int number_of_buffers, ...)
{
	_horizontalOffset = 0.0f;
	_verticalOffset = 0.0f;

	_numberOfBuffers = number_of_buffers;
	_bufferSizes = new int[_numberOfBuffers];
	_buffers = new uint8_t *[_numberOfBuffers];

	va_list va;
	va_start(va, number_of_buffers);
	for(int c = 0; c < _numberOfBuffers; c++)
	{
		_bufferSizes[c] = va_arg(va, int);
		_buffers[c] = new uint8_t[bufferHeight * bufferWidth * _bufferSizes[c]];
	}
	va_end(va);

	_write_allocation_pointer = 0;
	_cycles_per_line = cycles_per_line;
	_expected_next_hsync = cycles_per_line;
	_hsync_error_window = cycles_per_line >> 5;
	_horizontal_counter = 0;
	_sync_capacitor_charge_level = 0;

	_is_in_sync = false;
}

CRT::~CRT()
{
	delete[] _bufferSizes;
	for(int c = 0; c < _numberOfBuffers; c++)
	{
		delete[] _buffers[c];
	}
	delete[] _buffers;
}

#pragma mark - Sync loop

CRT::SyncEvent CRT::advance_to_next_sync_event(bool hsync_is_requested, bool vsync_is_charging, int cycles_to_run_for, int *cycles_advanced)
{
	// do we recognise this hsync, thereby adjusting time expectations?
	if ((_horizontal_counter < _hsync_error_window || _horizontal_counter >= _expected_next_hsync - _hsync_error_window) && hsync_is_requested) {
		_did_detect_hsync = true;

		int time_now = (_horizontal_counter < _hsync_error_window) ? _expected_next_hsync + _horizontal_counter : _horizontal_counter;
		_expected_next_hsync = (_expected_next_hsync + time_now) >> 1;
//		printf("to %d for %d\n", _expected_next_hsync, time_now);
	}

	SyncEvent proposedEvent = SyncEvent::None;
	int proposedSyncTime = cycles_to_run_for;

	// will we end an ongoing hsync?
	const int endOfHSyncTime = (millisecondsHorizontalRetraceTime*_cycles_per_line) >> 6;
	if (_horizontal_counter < endOfHSyncTime && _horizontal_counter+proposedSyncTime >= endOfHSyncTime) {
		proposedSyncTime = endOfHSyncTime - _horizontal_counter;
		proposedEvent = SyncEvent::EndHSync;
	}

	// will we start an hsync?
	if (_horizontal_counter + proposedSyncTime >= _expected_next_hsync) {
		proposedSyncTime = _expected_next_hsync - _horizontal_counter;
		proposedEvent = SyncEvent::StartHSync;
	}

	// will an acceptable vertical sync be triggered?
	if (vsync_is_charging && !_vretrace_counter) {
		const int startOfVSyncTime = syncCapacityLineChargeThreshold*_cycles_per_line;

		 if (_sync_capacitor_charge_level < startOfVSyncTime && _sync_capacitor_charge_level + proposedSyncTime >= startOfVSyncTime) {
			proposedSyncTime = startOfVSyncTime - _sync_capacitor_charge_level;
			proposedEvent = SyncEvent::StartVSync;
		 }
	}

	// TODO: will a late-in-the-day vertical sync be forced?

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

void CRT::advance_cycles(int number_of_cycles, bool hsync_requested, const bool vsync_charging, const CRTRun::Type type)
{
	while(number_of_cycles) {
		int next_run_length;
		SyncEvent next_event = advance_to_next_sync_event(hsync_requested, vsync_charging, number_of_cycles, &next_run_length);

		for(int c = 0; c < next_run_length; c++)
		{
			switch(type)
			{
				case CRTRun::Type::Data: putc('-', stdout);		break;
				case CRTRun::Type::Blank: putc(' ', stdout);	break;
				case CRTRun::Type::Level: putc('_', stdout);	break;
				case CRTRun::Type::Sync: putc('<', stdout);		break;
			}
		}
//		printf("[[%d]%d:%d]", type, next_event, next_run_length);

		hsync_requested = false;

		number_of_cycles -= next_run_length;
		_horizontal_counter += next_run_length;
		if (vsync_charging)
			_sync_capacitor_charge_level += next_run_length;
		else
			_sync_capacitor_charge_level = std::max(_sync_capacitor_charge_level - next_run_length, 0);

		_vretrace_counter = std::max(_vretrace_counter - next_run_length, 0);

		switch(next_event) {
			default:		break;
			case SyncEvent::StartHSync:
				_horizontal_counter = 0;
				printf("\n");
			break;

			case SyncEvent::EndHSync:
				if (!_did_detect_hsync) {
					_expected_next_hsync = (_expected_next_hsync + (_hsync_error_window >> 1) + _cycles_per_line) >> 1;
				}
				_did_detect_hsync = false;
			break;
			case SyncEvent::StartVSync:
				_vretrace_counter = scanlinesVerticalRetraceTime * _cycles_per_line;
				printf("\n\n===\n\n");
			break;
		}
	}
}

#pragma mark - stream feeding methods

void CRT::output_sync(int number_of_cycles)
{
	bool _hsync_requested = !_is_in_sync;
	_is_in_sync = true;
	advance_cycles(number_of_cycles, _hsync_requested, true, CRTRun::Type::Sync);
}

void CRT::output_level(int number_of_cycles, std::string type)
{
	_is_in_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Level);
}

void CRT::output_data(int number_of_cycles, std::string type)
{
	_is_in_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Data);
}

void CRT::output_blank(int number_of_cycles, std::string type)
{
	_is_in_sync = false;
	advance_cycles(number_of_cycles, false, false, CRTRun::Type::Blank);
}

#pragma mark - Buffer supply

void CRT::allocate_write_area(int required_length)
{
	int xPos = _write_allocation_pointer & (bufferWidth - 1);
	if (xPos + required_length > bufferWidth)
	{
		_write_allocation_pointer &= ~(bufferWidth - 1);
		_write_allocation_pointer = (_write_allocation_pointer + bufferWidth) & ((bufferHeight-1) * bufferWidth);
	}

	_write_target_pointer = _write_allocation_pointer;
	_write_allocation_pointer += required_length;
}

uint8_t *CRT::get_write_target_for_buffer(int buffer)
{
	return &_buffers[buffer][_write_target_pointer * _bufferSizes[buffer]];
}
