//
//  DigitalPhaseLockedLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DigitalPhaseLockedLoop.hpp"

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

	if(_samples_collected < _length_of_history)
	{
		_samples_collected++;
	}
	else
	{
		// perform a linear regression
		int sum_xy = 0;
		int sum_x = 0;
		int sum_y = 0;
		int sum_x_squared = 0;
		for(size_t pulse = 0; pulse < _length_of_history; pulse++)
		{
			int x = _pulse_history_array[pulse] / (int)_current_window_length;
			int y = _pulse_history_array[pulse] % (int)_current_window_length;

			sum_xy += x*y;
			sum_x += x;
			sum_y += y;
			sum_x_squared += x*x;
		}

		int gradient = (_length_of_history*sum_xy - sum_x*sum_y) / (_length_of_history*sum_x_squared - sum_x*sum_x);
		_current_window_length += gradient / 2;
		if(_current_window_length < _clocks_per_bit - _tolerance) _current_window_length = _clocks_per_bit - _tolerance;
		if(_current_window_length > _clocks_per_bit + _tolerance) _current_window_length = _clocks_per_bit + _tolerance;
	}

	// therefore, there was a 1 in this window
	_window_was_filled = true;
	if(_delegate) _delegate->digital_phase_locked_loop_output_bit(1);

	// shift history one to the left, storing new value; act as though the outgoing pulse were exactly halfway through its
	// window for adjustment purposes
	int outgoing_pulse_time = _pulse_history_array[0];//_pulse_history_array[0] + (_current_window_length >> 1);

	for(size_t pulse = 1; pulse < _length_of_history; pulse++)
	{
		_pulse_history_array[pulse - 1] = _pulse_history_array[pulse] - outgoing_pulse_time;
	}
	_next_pulse_time -= outgoing_pulse_time;
	_pulse_history_array[_length_of_history-1] = _next_pulse_time;
}
