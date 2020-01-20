//
//  TimedEventLoop.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "TimedEventLoop.hpp"
#include "../Numeric/Factors.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

using namespace Storage;

TimedEventLoop::TimedEventLoop(Cycles::IntType input_clock_rate) :
	input_clock_rate_(input_clock_rate) {}

void TimedEventLoop::run_for(const Cycles cycles) {
	auto remaining_cycles = cycles.as_integral();
#ifndef NDEBUG
	decltype(remaining_cycles) cycles_advanced = 0;
#endif

	while(cycles_until_event_ <= remaining_cycles) {
#ifndef NDEBUG
		cycles_advanced += cycles_until_event_;
#endif
		advance(cycles_until_event_);
		remaining_cycles -= cycles_until_event_;
		cycles_until_event_ = 0;
		process_next_event();
	}

	if(remaining_cycles) {
		cycles_until_event_ -= remaining_cycles;
#ifndef NDEBUG
		cycles_advanced += remaining_cycles;
#endif
		advance(remaining_cycles);
	}

	assert(cycles_advanced == cycles.as_integral());
	assert(cycles_until_event_ > 0);
}

Cycles::IntType TimedEventLoop::get_cycles_until_next_event() const {
	return std::max(cycles_until_event_, Cycles::IntType(0));
}

Cycles::IntType TimedEventLoop::get_input_clock_rate() const {
	return input_clock_rate_;
}

void TimedEventLoop::reset_timer() {
	subcycles_until_event_ = 0.0;
	cycles_until_event_ = 0;
}

void TimedEventLoop::jump_to_next_event() {
	reset_timer();
	process_next_event();
}

void TimedEventLoop::set_next_event_time_interval(Time interval) {
	set_next_event_time_interval(interval.get<float>());
}

void TimedEventLoop::set_next_event_time_interval(float interval) {
	// Calculate [interval]*[input clock rate] + [subcycles until this event]
	float float_interval = interval * float(input_clock_rate_) + subcycles_until_event_;

	// So this event will fire in the integral number of cycles from now, putting us at the remainder
	// number of subcycles
	const Cycles::IntType addition = Cycles::IntType(float_interval);
	cycles_until_event_ += addition;
	subcycles_until_event_ = fmodf(float_interval, 1.0);

	assert(cycles_until_event_ >= 0);
	assert(subcycles_until_event_ >= 0.0);
}

Time TimedEventLoop::get_time_into_next_event() {
	// TODO: calculate, presumably as [length of interval] - ([cycles left] + [subcycles left])
	Time zero;
	return zero;
}
