//
//  TimedEventLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TimedEventLoop.hpp"
#include "../NumberTheory/Factors.hpp"
#include <algorithm>

using namespace Storage;

TimedEventLoop::TimedEventLoop(unsigned int input_clock_rate) :
	_input_clock_rate(input_clock_rate) {}

void TimedEventLoop::run_for_cycles(int number_of_cycles)
{
	_cycles_until_event -= number_of_cycles;
	while(_cycles_until_event <= 0)
	{
		process_next_event();
	}
}

unsigned int TimedEventLoop::get_cycles_until_next_event()
{
	return (unsigned int)std::max(_cycles_until_event, 0);
}

void TimedEventLoop::reset_timer()
{
	_error.set_zero();
	_cycles_until_event = 0;
}

void TimedEventLoop::reset_timer_to_offset(Time offset)
{
/*	unsigned int common_clock_rate = NumberTheory::least_common_multiple(offset.clock_rate, _event_interval.clock_rate);
	_time_into_interval = offset.length * (common_clock_rate / offset.clock_rate);
	_event_interval.length *= common_clock_rate / _event_interval.clock_rate;
	_event_interval.clock_rate = common_clock_rate;
	if(common_clock_rate != _stepper->get_output_rate())
	{
		_stepper.reset(new SignalProcessing::Stepper(_event_interval.clock_rate, _input_clock_rate));
	}*/
}

void TimedEventLoop::jump_to_next_event()
{
	reset_timer();
	process_next_event();
}

void TimedEventLoop::set_next_event_time_interval(Time interval)
{
	unsigned int common_divisor = NumberTheory::greatest_common_divisor(_error.clock_rate, interval.clock_rate);
	uint64_t denominator = (interval.clock_rate * _error.clock_rate) / common_divisor;
	uint64_t numerator = (_error.clock_rate / common_divisor) * _input_clock_rate * interval.length - (interval.clock_rate / common_divisor) * _error.length;

	_cycles_until_event = (int)(numerator / denominator);
	_error.length = (unsigned int)(numerator % denominator);
	_error.clock_rate = (unsigned int)denominator;
	_error.simplify();
}

Time TimedEventLoop::get_time_into_next_event()
{
	Time result = _event_interval;
//	result.length = _time_into_interval;
	return result;
}
