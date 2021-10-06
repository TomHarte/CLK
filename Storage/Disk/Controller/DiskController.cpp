//
//  DiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "DiskController.hpp"

using namespace Storage::Disk;

Controller::Controller(Cycles clock_rate) :
		clock_rate_multiplier_(128000000 / clock_rate.as_integral()),
		clock_rate_(clock_rate.as_integral() * clock_rate_multiplier_),
		pll_(100, *this),
		empty_drive_(int(clock_rate.as_integral()), 1, 1),
		drive_(&empty_drive_) {
	empty_drive_.set_clocking_hint_observer(this);
	set_expected_bit_length(Time(1));
}

void Controller::set_component_prefers_clocking(ClockingHint::Source *, ClockingHint::Preference) {
	update_clocking_observer();
}

ClockingHint::Preference Controller::preferred_clocking() const {
	// Nominate RealTime clocking if any drive currently wants any clocking whatsoever.
	// Otherwise, ::None will do.
	for(auto &drive: drives_) {
		const auto preferred_clocking = drive.preferred_clocking();
		if(preferred_clocking != ClockingHint::Preference::None) {
			return ClockingHint::Preference::RealTime;
		}
	}
	if(empty_drive_.preferred_clocking() != ClockingHint::Preference::None) {
		return ClockingHint::Preference::RealTime;
	}
	return ClockingHint::Preference::None;
}

void Controller::run_for(const Cycles cycles) {
	for(auto &drive: drives_) {
		drive.run_for(cycles);
	}
	empty_drive_.run_for(cycles);
}

Drive &Controller::get_drive() {
	return *drive_;
}

// MARK: - Drive::EventDelegate

void Controller::process_event(const Drive::Event &event) {
	switch(event.type) {
		case Track::Event::FluxTransition:	pll_.add_pulse();		break;
		case Track::Event::IndexHole:		process_index_hole();	break;
	}
}

void Controller::advance(const Cycles cycles) {
	if(is_reading_) pll_.run_for(Cycles(cycles.as_integral() * clock_rate_multiplier_));
}

void Controller::process_write_completed() {
	// Provided for subclasses to override.
}

// MARK: - PLL control and delegate

void Controller::set_expected_bit_length(Time bit_length) {
	bit_length_ = bit_length;
	bit_length_.simplify();

	Time cycles_per_bit = Storage::Time(int(clock_rate_)) * bit_length;
	cycles_per_bit.simplify();

	// this conversion doesn't need to be exact because there's a lot of variation to be taken
	// account of in rotation speed, air turbulence, etc, so a direct conversion will do
	const int clocks_per_bit = cycles_per_bit.get<int>();
	pll_.set_clocks_per_bit(clocks_per_bit);
}

void Controller::digital_phase_locked_loop_output_bit(int value) {
	if(is_reading_) process_input_bit(value);
}

void Controller::set_drive(int index_mask) {
	if(drive_selection_mask_ == index_mask) {
		return;
	}

	const ClockingHint::Preference former_preference = preferred_clocking();

	// Stop receiving events from the current drive.
	get_drive().set_event_delegate(nullptr);

	// TODO: a transfer of writing state, if writing?

	if(!index_mask) {
		drive_ = &empty_drive_;
	} else {
		// TEMPORARY FIX: connect up only the first selected drive.
		// TODO: at least merge events if multiple drives are selected. Some computers have
		// controllers that allow this, with usually meaningless results as far as I can
		// imagine. But the limit of an emulator shouldn't be the author's imagination.
		size_t index = 0;
		while(!(index_mask&1)) {
			index_mask >>= 1;
			++index;
		}
		drive_ = &drives_[index];
	}

	get_drive().set_event_delegate(this);

	if(preferred_clocking() != former_preference) {
		update_clocking_observer();
	}
}

void Controller::begin_writing(bool clamp_to_index_hole) {
	is_reading_ = false;
	get_drive().begin_writing(bit_length_, clamp_to_index_hole);
}

void Controller::end_writing() {
	if(!is_reading_) {
		is_reading_ = true;
		get_drive().end_writing();
	}
}

bool Controller::is_reading() {
	return is_reading_;
}
