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
	set_expected_bit_length(Time(1));
}

void Controller::set_component_prefers_clocking(ClockingHint::Source *component, ClockingHint::Preference clocking) {
	update_clocking_observer();
}

ClockingHint::Preference Controller::preferred_clocking() {
	return (!drive_ || (drive_->preferred_clocking() == ClockingHint::Preference::None)) ? ClockingHint::Preference::None : ClockingHint::Preference::RealTime;
}

void Controller::run_for(const Cycles cycles) {
	if(drive_) drive_->run_for(cycles);
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

	ClockingHint::Preference former_prefernece = preferred_clocking();
//	invalidate_track();

	get_drive().set_event_delegate(nullptr);
	get_drive().set_clocking_hint_observer(nullptr);

	if(!index_mask) {
		drive_ = &empty_drive_;
	} else {
		size_t index = 0;
		while(!(index_mask&1)) {
			index_mask >>= 1;
			++index;
		}
		drive_ = &drives_[index];
	}

	get_drive().set_event_delegate(this);
	get_drive().set_clocking_hint_observer(this);

	if(preferred_clocking() != former_prefernece) {
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
