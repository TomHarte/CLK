//
//  TimedEventLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TimedEventLoop.hpp"
#include "../NumberTheory/Factors.hpp"

using namespace Storage;

TimedEventLoop::TimedEventLoop(unsigned int input_clock_rate) :
	_input_clock_rate(input_clock_rate) {}

void TimedEventLoop::run_for_cycles(unsigned int number_of_cycles)
{
	_time_into_interval += (unsigned int)_stepper->step(number_of_cycles);
	while(_time_into_interval >= _event_interval.length)
	{
		process_next_event();
	}
}

void TimedEventLoop::reset_timer()
{
	_time_into_interval = 0;
	_stepper.reset();
}

void TimedEventLoop::jump_to_next_event()
{
	reset_timer();
	process_next_event();
}

void TimedEventLoop::set_next_event_time_interval(Time interval)
{
	// figure out how much time has been run since the last bit ended
	if(_stepper)
	{
		_time_into_interval -= _event_interval.length;
		if(_time_into_interval)
		{
			// simplify the quotient
			unsigned int common_divisor = NumberTheory::greatest_common_divisor(_time_into_interval, _event_interval.clock_rate);
			_time_into_interval /= common_divisor;
			_event_interval.clock_rate /= common_divisor;

			// build a quotient that is the sum of the time overrun plus the incoming time and adjust the time overrun
			// to be in terms of the new quotient
			unsigned int denominator = NumberTheory::least_common_multiple(_event_interval.clock_rate, interval.clock_rate);
			interval.length *= denominator / interval.clock_rate;
			interval.clock_rate = denominator;
			_time_into_interval *= denominator / _event_interval.clock_rate;
		}
	}
	else
	{
		_time_into_interval = 0;
	}

	// store new interval
	_event_interval = interval;

	// adjust stepper if required
	if(!_stepper || _event_interval.clock_rate != _stepper->get_output_rate())
	{
		_stepper.reset(new SignalProcessing::Stepper(_event_interval.clock_rate, _input_clock_rate));
	}
}
