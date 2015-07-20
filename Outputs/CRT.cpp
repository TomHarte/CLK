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
	_hsync_error_window = cycles_per_line / 10;
	_horizontal_counter = 0;
	_sync_capacitor_charge_level = 0;

	_hretrace_counter = -1;
	_is_in_sync = false;
	_vsync_is_proposed = false;
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

#pragma mark - Sync decisions

#define hretrace_period() ((millisecondsHorizontalRetraceTime * _cycles_per_line) >> 6)

void CRT::propose_hsync()
{
	if (_horizontal_counter >= _expected_next_hsync - _hsync_error_window)
	{
		_expected_next_hsync = (_horizontal_counter + _expected_next_hsync) >> 1;
		do_hsync();
	}
	else
	{
		printf("r %d\n", _horizontal_counter);
	}
}

void CRT::charge_vsync(int number_of_cycles)
{
	// will we start indicating hsync during this charge?
	const int final_capacitor_charge_level = _sync_capacitor_charge_level + number_of_cycles;
	const int required_capacitor_charge_level = syncCapacityLineChargeThreshold*_cycles_per_line;
	if(_sync_capacitor_charge_level < required_capacitor_charge_level && final_capacitor_charge_level >= required_capacitor_charge_level)
	{
		const int cycles_until_vsync_starts = required_capacitor_charge_level - _sync_capacitor_charge_level;
		run_line_for_cycles(cycles_until_vsync_starts);
		_vsync_is_proposed = true;
		run_line_for_cycles(number_of_cycles - cycles_until_vsync_starts);
	}
	else
	{
		run_line_for_cycles(number_of_cycles);
	}
	_sync_capacitor_charge_level += number_of_cycles;
}

void CRT::drain_vsync(int number_of_cycles)
{
	// will we stop indicating hsync during this charge?
	const int required_capacitor_charge_level = syncCapacityLineChargeThreshold*_cycles_per_line;
	if(_sync_capacitor_charge_level >= required_capacitor_charge_level && _sync_capacitor_charge_level - number_of_cycles < required_capacitor_charge_level)
	{
		const int cycles_until_vsync_ends = _sync_capacitor_charge_level - required_capacitor_charge_level;
		run_line_for_cycles(cycles_until_vsync_ends);
		_vsync_is_proposed = false;
		run_line_for_cycles(number_of_cycles - cycles_until_vsync_ends);
	}
	else
	{
		run_line_for_cycles(number_of_cycles);
	}
	_sync_capacitor_charge_level = std::max(0, _sync_capacitor_charge_level - number_of_cycles);
}

void CRT::run_line_for_cycles(int number_of_cycles)
{
	// we're guaranteed not to see any vertical sync events during this run_for_cycles;
	// will we see a horizontal?

	if(!_hretrace_counter)
	{
		const int end_counter = _horizontal_counter + number_of_cycles;
		const int last_allowed_retrace_time = _expected_next_hsync + _hsync_error_window;
		if(end_counter >= last_allowed_retrace_time)
		{
			// there'll be a forced retrace, and we didn't detect a sync pulse so we'll
			// push back towards the default period
			const int cycles_before_retrace = end_counter - last_allowed_retrace_time;
			run_hline_for_cycles(cycles_before_retrace);
			do_hsync();
			_hretrace_counter = hretrace_period();
			_expected_next_hsync = (_expected_next_hsync + _cycles_per_line) >> 1;
			run_hline_for_cycles(number_of_cycles - cycles_before_retrace);
		}
		else
		{
			// we'll just output, no big deal
			run_hline_for_cycles(number_of_cycles);
		}
	}
	else
	{
		if(_hretrace_counter - number_of_cycles < 0)
		{
			// we'll fully retrace and exit
			number_of_cycles -= _hretrace_counter;
			run_hline_for_cycles(number_of_cycles - _hretrace_counter);
			_hretrace_counter = 0;
		}
		else
		{
			// we'll spend this whole period retracing
			_hretrace_counter -= number_of_cycles;
		}

		_hretrace_counter = std::max(0, _hretrace_counter - number_of_cycles);
	}
}

void CRT::run_hline_for_cycles(int number_of_cycles)
{
	_horizontal_counter += number_of_cycles;
}

void CRT::do_hsync()
{
	printf("%d\n", _horizontal_counter);
	_hretrace_counter = hretrace_period();
	_horizontal_counter = 0;
}


#pragma mark - stream feeding methods

void CRT::output_sync(int number_of_cycles)
{
//	printf("[%d]\n", number_of_cycles);
//
//	if(number_of_cycles > 16)
//	{
//		printf("!!!\n");
//	}

	// horizontal sync is edge triggered
	if(!_is_in_sync)
	{
		_is_in_sync = true;
		propose_hsync();
	}
	charge_vsync(number_of_cycles);
}

void CRT::output_level(int number_of_cycles, std::string type)
{
	_is_in_sync = false;
	drain_vsync(number_of_cycles);
}

void CRT::output_data(int number_of_cycles, std::string type)
{
	_is_in_sync = false;
	drain_vsync(number_of_cycles);
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
