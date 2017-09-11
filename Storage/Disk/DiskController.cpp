//
//  DiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DiskController.hpp"

#include "../../NumberTheory/Factors.hpp"

using namespace Storage::Disk;

Controller::Controller(Cycles clock_rate) :
		clock_rate_multiplier_(128000000 / clock_rate.as_int()),
		clock_rate_(clock_rate.as_int() * clock_rate_multiplier_),
		empty_drive_(new Drive((unsigned int)clock_rate.as_int(), 1)) {
	// seed this class with a PLL, any PLL, so that it's safe to assume non-nullptr later
	Time one(1);
	set_expected_bit_length(one);
	set_drive(empty_drive_);
}

void Controller::set_component_is_sleeping(void *component, bool is_sleeping) {
	update_sleep_observer();
}

bool Controller::is_sleeping() {
	return !drive_ || drive_->is_sleeping();
}

void Controller::run_for(const Cycles cycles) {
	if(drive_) drive_->run_for(cycles);
}

Drive &Controller::get_drive() {
	return *drive_.get();
}

#pragma mark - Drive::EventDelegate

void Controller::process_event(const Track::Event &event) {
	switch(event.type) {
		case Track::Event::FluxTransition:	pll_->add_pulse();		break;
		case Track::Event::IndexHole:		process_index_hole();	break;
	}
}

void Controller::advance(const Cycles cycles) {
	pll_->run_for(Cycles(cycles.as_int() * clock_rate_multiplier_));
}

void Controller::process_write_completed() {
	// Provided for subclasses to override.
}

#pragma mark - PLL control and delegate

void Controller::set_expected_bit_length(Time bit_length) {
	bit_length_ = bit_length;
	bit_length_.simplify();

	Time cycles_per_bit = Storage::Time(clock_rate_) * bit_length;
	cycles_per_bit.simplify();

	// this conversion doesn't need to be exact because there's a lot of variation to be taken
	// account of in rotation speed, air turbulence, etc, so a direct conversion will do
	int clocks_per_bit = (int)cycles_per_bit.get_unsigned_int();
	pll_.reset(new DigitalPhaseLockedLoop(clocks_per_bit, 3));
	pll_->set_delegate(this);
}

void Controller::digital_phase_locked_loop_output_bit(int value) {
	process_input_bit(value);
}

void Controller::set_drive(std::shared_ptr<Drive> drive) {
	if(drive_ != drive) {
		bool was_sleeping = is_sleeping();
//		invalidate_track();

		if(drive_) {
			drive_->set_event_delegate(nullptr);
			drive_->set_sleep_observer(nullptr);
		}
		drive_ = drive;
		if(drive_) {
			drive_->set_event_delegate(this);
			drive_->set_sleep_observer(this);
		} else {
			drive_ = empty_drive_;
		}

		if(is_sleeping() != was_sleeping) {
			update_sleep_observer();
		}
	}
}

void Controller::begin_writing(bool clamp_to_index_hole) {
	get_drive().begin_writing(bit_length_, clamp_to_index_hole);
}
