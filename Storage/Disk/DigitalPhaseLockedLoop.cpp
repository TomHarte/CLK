//
//  DigitalPhaseLockedLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DigitalPhaseLockedLoop.hpp"
#include <algorithm>
#include <cstdlib>

using namespace Storage;

DigitalPhaseLockedLoop::DigitalPhaseLockedLoop(int clocks_per_bit, int tolerance, int length_of_history) :
	_clocks_per_bit(clocks_per_bit),
	_tolerance(tolerance),
	_length_of_history(length_of_history),
	_pulse_history(new int[length_of_history]),
	_current_window_length(clocks_per_bit),
	_next_pulse_time(0),
	_window_was_filled(false),
	_window_offset(0),
	_samples_collected(0) {}

void DigitalPhaseLockedLoop::run_for_cycles(int number_of_cycles)
{
	// check whether this triggers any 0s
	_window_offset += number_of_cycles;
	if(_delegate)
	{
		while(_window_offset > _current_window_length)
		{
			if(!_window_was_filled) _delegate->digital_phase_locked_loop_output_bit(0);
			_window_was_filled = false;
			_window_offset -= _current_window_length;
		}
	}
	else
	{
		_window_offset %= _current_window_length;
	}

	// update timing
	_next_pulse_time += number_of_cycles;
}

void DigitalPhaseLockedLoop::add_pulse()
{
	int *const _pulse_history_array = (int *)_pulse_history.get();
	int outgoing_pulse_time = 0;

	if(_samples_collected <= _length_of_history)
	{
		_samples_collected++;
	}
	else
	{
		outgoing_pulse_time	= _pulse_history_array[0];

		// temporary: perform an exhaustive search for the ideal window length
		int minimum_error = __INT_MAX__;
		int ideal_length = 0;
		for(int c = _clocks_per_bit - _tolerance; c < _clocks_per_bit + _tolerance; c++)
		{
			int total_error = 0;
			const int half_window = c >> 1;
			for(size_t pulse = 1; pulse < _length_of_history; pulse++)
			{
				int difference = _pulse_history_array[pulse] - _pulse_history_array[pulse-1];
				difference += half_window;
				const int steps = difference / c;
				const int offset = difference%c - half_window;

				total_error += abs(offset / steps);
			}
			if(total_error < minimum_error)
			{
				minimum_error = total_error;
				ideal_length = c;
			}
		}

		// use a spring mechanism to effect a lowpass filter
		_current_window_length = ((ideal_length + (_current_window_length*3)) + 2) >> 2;
	}

	// therefore, there was a 1 in this window
	_window_was_filled = true;
	if(_delegate) _delegate->digital_phase_locked_loop_output_bit(1);

	// shift history one to the left, storing new value, potentially with a centring adjustment
	for(size_t pulse = 1; pulse < _length_of_history; pulse++)
	{
		_pulse_history_array[pulse - 1] = _pulse_history_array[pulse] - outgoing_pulse_time;
	}
	_next_pulse_time -= outgoing_pulse_time;
	_pulse_history_array[_length_of_history-1] = _next_pulse_time;
}
