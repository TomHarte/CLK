//
//  DigitalPhaseLockedLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DigitalPhaseLockedLoop.hpp"

using namespace Storage;

DigitalPhaseLockedLoop::DigitalPhaseLockedLoop(unsigned int clocks_per_bit, unsigned int tolerance, unsigned int length_of_history) :
	_clocks_per_bit(clocks_per_bit),
	_tolerance(tolerance),
	_length_of_history(length_of_history),
	_pulse_history(new unsigned int[length_of_history]),
	_current_window_length(clocks_per_bit),
	_next_pulse_time(0),
	_window_was_filled(false),
	_window_offset(0) {}

void DigitalPhaseLockedLoop::run_for_cycles(unsigned int number_of_cycles)
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

	// updte timing
	_next_pulse_time += number_of_cycles;
}

void DigitalPhaseLockedLoop::add_pulse()
{
	// TODO: linear regression to adjust _current_window_length and _next_pulse_time


	// therefore, there was a 1 in this window
	_window_was_filled = true;
	if(_delegate) _delegate->digital_phase_locked_loop_output_bit(1);

	// shift history one to the left, subtracting the outgoing pulse from
	unsigned int *const _pulse_history_array = _pulse_history.get();
	unsigned int outgoing_pulse_time = _pulse_history_array[0];

	for(size_t pulse = 1; pulse < _length_of_history; pulse++)
	{
		_pulse_history_array[pulse - 1] = _pulse_history_array[pulse] - outgoing_pulse_time;
	}
	_next_pulse_time -= outgoing_pulse_time;
	_pulse_history_array[_length_of_history-1] = _next_pulse_time;
}
