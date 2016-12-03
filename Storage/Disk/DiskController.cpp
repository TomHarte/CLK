//
//  DiskController.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DiskController.hpp"

using namespace Storage::Disk;

Controller::Controller(unsigned int clock_rate, unsigned int clock_rate_multiplier, unsigned int revolutions_per_minute) :
	clock_rate_(clock_rate * clock_rate_multiplier),
	clock_rate_multiplier_(clock_rate_multiplier),

	TimedEventLoop(clock_rate * clock_rate_multiplier)
{
	rotational_multiplier_.length = 60;
	rotational_multiplier_.clock_rate = revolutions_per_minute;
	rotational_multiplier_.simplify();

	// seed this class with a PLL, any PLL, so that it's safe to assume non-nullptr later
	Time one;
	set_expected_bit_length(one);
}

void Controller::setup_track()
{
	track_ = drive_->get_track();

	Time offset;
	if(track_ && time_into_track_.length > 0)
	{
		Time time_found = track_->seek_to(time_into_track_).simplify();
		offset = (time_into_track_ - time_found).simplify();
		time_into_track_ = time_found;
	}
	else
	{
		offset = time_into_track_;
		time_into_track_.set_zero();
	}

	reset_timer_to_offset(offset * rotational_multiplier_);
	get_next_event();
}

void Controller::run_for_cycles(int number_of_cycles)
{
	if(drive_ && drive_->has_disk() && motor_is_on_)
	{
		if(!track_) setup_track();
		number_of_cycles *= clock_rate_multiplier_;
		while(number_of_cycles)
		{
			int cycles_until_next_event = (int)get_cycles_until_next_event();
			int cycles_to_run_for = std::min(cycles_until_next_event, number_of_cycles);
			cycles_since_index_hole_ += (unsigned int)cycles_to_run_for;
			number_of_cycles -= cycles_to_run_for;
			pll_->run_for_cycles(cycles_to_run_for);
			TimedEventLoop::run_for_cycles(cycles_to_run_for);
		}
	}
}

#pragma mark - Track timed event loop

void Controller::get_next_event()
{
	if(track_)
		current_event_ = track_->get_next_event();
	else
	{
		current_event_.length.length = 1;
		current_event_.length.clock_rate = 1;
		current_event_.type = Track::Event::IndexHole;
	}

	// divide interval, which is in terms of a rotation of the disk, by rotation speed, and
	// convert it into revolutions per second
	set_next_event_time_interval(current_event_.length * rotational_multiplier_);
}

void Controller::process_next_event()
{
	switch(current_event_.type)
	{
		case Track::Event::FluxTransition:
			pll_->add_pulse();
			time_into_track_ += current_event_.length;
		break;
		case Track::Event::IndexHole:
			cycles_since_index_hole_ = 0;
			time_into_track_.set_zero();
			process_index_hole();
		break;
	}
	get_next_event();
}

#pragma mark - PLL control and delegate

void Controller::set_expected_bit_length(Time bit_length)
{
	bit_length_ = bit_length;

	// this conversion doesn't need to be exact because there's a lot of variation to be taken
	// account of in rotation speed, air turbulence, etc, so a direct conversion will do
	int clocks_per_bit = (int)((bit_length.length * clock_rate_) / bit_length.clock_rate);
	pll_.reset(new DigitalPhaseLockedLoop(clocks_per_bit, clocks_per_bit / 5, 3));
	pll_->set_delegate(this);
}

void Controller::digital_phase_locked_loop_output_bit(int value)
{
	process_input_bit(value, cycles_since_index_hole_);
}

#pragma mark - Drive actions

bool Controller::get_is_track_zero()
{
	if(!drive_) return false;
	return drive_->get_is_track_zero();
}

bool Controller::get_drive_is_ready()
{
	if(!drive_) return false;
	return drive_->has_disk();
}

void Controller::step(int direction)
{
	if(drive_) drive_->step(direction);
	invalidate_track();
}

void Controller::set_motor_on(bool motor_on)
{
	motor_is_on_ = motor_on;
}

bool Controller::get_motor_on()
{
	return motor_is_on_;
}

void Controller::set_drive(std::shared_ptr<Drive> drive)
{
	drive_ = drive;
	invalidate_track();
}

void Controller::invalidate_track()
{
	track_ = nullptr;
}
