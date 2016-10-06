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
	_subcycles_until_event.set_zero();
	_cycles_until_event = 0;
}

void TimedEventLoop::reset_timer_to_offset(Time offset)
{
	// TODO: apply
}

void TimedEventLoop::jump_to_next_event()
{
	reset_timer();
	process_next_event();
}

void TimedEventLoop::set_next_event_time_interval(Time interval)
{
	// Calculate [interval]*[input clock rate] + [subcycles until this event].
	int64_t denominator = (int64_t)interval.clock_rate * (int64_t)_subcycles_until_event.clock_rate;
	int64_t numerator =
		(int64_t)_subcycles_until_event.clock_rate * (int64_t)_input_clock_rate * (int64_t)interval.length +
		(int64_t)interval.clock_rate * (int64_t)_subcycles_until_event.length;

	// Simplify now, to prepare for stuffing into possibly 32-bit quantities
	int64_t common_divisor = NumberTheory::greatest_common_divisor(numerator % denominator, denominator);
	denominator /= common_divisor;
	numerator /= common_divisor;

	// So this event will fire in the integral number of cycles from now, putting us at the remainder
	// number of subcycles
	_cycles_until_event = (int)(numerator / denominator);
	_subcycles_until_event.length = (unsigned int)(numerator % denominator);
	_subcycles_until_event.clock_rate = (unsigned int)denominator;
}

Time TimedEventLoop::get_time_into_next_event()
{
	// TODO: calculate, presumably as [length of interval] - ([cycles left] + [subcycles left]) 
	Time zero;
	return zero;
}
