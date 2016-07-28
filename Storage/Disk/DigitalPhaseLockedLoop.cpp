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

DigitalPhaseLockedLoop::DigitalPhaseLockedLoop(int clocks_per_bit, int tolerance, size_t length_of_history) :
	_clocks_per_bit(clocks_per_bit),
	_tolerance(tolerance),

	_phase(0),
	_window_length(clocks_per_bit),

	_phase_error_pointer(0)
{
	_phase_error_history.reset(new std::vector<int>(length_of_history, 0));
}

void DigitalPhaseLockedLoop::run_for_cycles(int number_of_cycles)
{
	// check whether this triggers any 0s
	_phase += number_of_cycles;
	if(_delegate)
	{
		while(_phase > _window_length)
		{
			if(!_window_was_filled) _delegate->digital_phase_locked_loop_output_bit(0);
			_window_was_filled = false;
			_phase -= _window_length;
		}
	}
	else
	{
		_phase %= _window_length;
	}
}

void DigitalPhaseLockedLoop::add_pulse()
{
	if(!_window_was_filled)
	{
		if(_delegate) _delegate->digital_phase_locked_loop_output_bit(1);
		_window_was_filled = true;
		post_phase_error(_phase - (_window_length >> 1));
	}
}

void DigitalPhaseLockedLoop::post_phase_error(int error)
{
	// use a simple spring mechanism as a lowpass filter for phase
	_phase -= (error + 1) >> 1;

	// use the average of the last few errors to affect frequency
	std::vector<int> *phase_error_history = _phase_error_history.get();
	size_t phase_error_history_size = phase_error_history->size();

	(*phase_error_history)[_phase_error_pointer] = error;
	_phase_error_pointer = (_phase_error_pointer + 1)%phase_error_history_size;

	int total_error = 0;
	for(size_t c = 0; c < phase_error_history_size; c++)
	{
		total_error += (*phase_error_history)[c];
	}
	int denominator = (int)(phase_error_history_size * 4);
	_window_length += (total_error + (denominator >> 1)) / denominator;
	_window_length = std::max(std::min(_window_length, _clocks_per_bit + _tolerance), _clocks_per_bit - _tolerance);
}
