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

Controller::Controller(unsigned int clock_rate, unsigned int clock_rate_multiplier, unsigned int revolutions_per_minute) :
		clock_rate_(clock_rate * clock_rate_multiplier),
		clock_rate_multiplier_(clock_rate_multiplier),
		rotational_multiplier_(60u, revolutions_per_minute),

		cycles_since_index_hole_(0),
		motor_is_on_(false),

		is_reading_(true),

		TimedEventLoop(clock_rate * clock_rate_multiplier) {
	// seed this class with a PLL, any PLL, so that it's safe to assume non-nullptr later
	Time one(1);
	set_expected_bit_length(one);
}

void Controller::setup_track() {
	track_ = drive_->get_track();

	Time offset;
	Time track_time_now = get_time_into_track();
	if(track_) {
		Time time_found = track_->seek_to(track_time_now);
		offset = track_time_now - time_found;
	}

	get_next_event(offset);
}

void Controller::run_for_cycles(int number_of_cycles) {
	Time zero(0);

	if(drive_ && drive_->has_disk() && motor_is_on_) {
		if(!track_) setup_track();

		number_of_cycles *= clock_rate_multiplier_;
		while(number_of_cycles) {
			int cycles_until_next_event = (int)get_cycles_until_next_event();
			int cycles_to_run_for = std::min(cycles_until_next_event, number_of_cycles);
			if(!is_reading_ && cycles_until_bits_written_ > zero) {
				int write_cycles_target = (int)cycles_until_bits_written_.get_unsigned_int();
				if(cycles_until_bits_written_.length % cycles_until_bits_written_.clock_rate) write_cycles_target++;
				cycles_to_run_for = std::min(cycles_to_run_for, write_cycles_target);
			}

			cycles_since_index_hole_ += (unsigned int)cycles_to_run_for;

			number_of_cycles -= cycles_to_run_for;
			if(is_reading_) {
				pll_->run_for_cycles(cycles_to_run_for);
			} else {
				if(cycles_until_bits_written_ > zero) {
					Storage::Time cycles_to_run_for_time(cycles_to_run_for);
					if(cycles_until_bits_written_ <= cycles_to_run_for_time) {
						process_write_completed();
						if(cycles_until_bits_written_ <= cycles_to_run_for_time)
							cycles_until_bits_written_.set_zero();
						else
							cycles_until_bits_written_ -= cycles_to_run_for_time;
					} else {
						cycles_until_bits_written_ -= cycles_to_run_for_time;
					}
				}
			}
			TimedEventLoop::run_for_cycles(cycles_to_run_for);
		}
	}
}

#pragma mark - Track timed event loop

void Controller::get_next_event(const Time &duration_already_passed) {
	if(track_) {
		current_event_ = track_->get_next_event();
	} else {
		current_event_.length.length = 1;
		current_event_.length.clock_rate = 1;
		current_event_.type = Track::Event::IndexHole;
	}

	// divide interval, which is in terms of a single rotation of the disk, by rotation speed to
	// convert it into revolutions per second; this is achieved by multiplying by rotational_multiplier_
	set_next_event_time_interval((current_event_.length - duration_already_passed) * rotational_multiplier_);
}

void Controller::process_next_event()
{
	switch(current_event_.type) {
		case Track::Event::FluxTransition:
			if(is_reading_) pll_->add_pulse();
		break;
		case Track::Event::IndexHole:
			printf("%p %d [/%d = %d]\n", this, cycles_since_index_hole_, clock_rate_multiplier_, cycles_since_index_hole_ / clock_rate_multiplier_);
			cycles_since_index_hole_ = 0;
			process_index_hole();
		break;
	}
	get_next_event(Time(0));
}

Storage::Time Controller::get_time_into_track() {
	// this is proportion of a second
	Time result(cycles_since_index_hole_, 8000000 * clock_rate_multiplier_);
	result /= rotational_multiplier_;
	result.simplify();
	return result;
}

#pragma mark - Writing

void Controller::begin_writing() {
	is_reading_ = false;

	write_segment_.length_of_a_bit = bit_length_ / rotational_multiplier_;
	write_segment_.data.clear();
	write_segment_.number_of_bits = 0;

	write_start_time_ = get_time_into_track();
}

void Controller::write_bit(bool value) {
	bool needs_new_byte = !(write_segment_.number_of_bits&7);
	if(needs_new_byte) write_segment_.data.push_back(0);
	if(value) write_segment_.data[write_segment_.number_of_bits >> 3] |= 0x80 >> (write_segment_.number_of_bits & 7);
	write_segment_.number_of_bits++;

	cycles_until_bits_written_ += cycles_per_bit_;
}

void Controller::end_writing() {
	is_reading_ = true;

	if(!patched_track_) {
		// Avoid creating a new patched track if this one is already patched
		patched_track_ = std::dynamic_pointer_cast<PCMPatchedTrack>(track_);
		if(!patched_track_) {
			patched_track_.reset(new PCMPatchedTrack(track_));
		}
	}
	patched_track_->add_segment(write_start_time_, write_segment_);
	invalidate_track();	// TEMPORARY: to force a seek
}

#pragma mark - PLL control and delegate

void Controller::set_expected_bit_length(Time bit_length) {
	bit_length_ = bit_length;
	bit_length_.simplify();

	cycles_per_bit_ = Storage::Time(clock_rate_) * bit_length;
	cycles_per_bit_.simplify();

	// this conversion doesn't need to be exact because there's a lot of variation to be taken
	// account of in rotation speed, air turbulence, etc, so a direct conversion will do
	int clocks_per_bit = (int)cycles_per_bit_.get_unsigned_int();
	pll_.reset(new DigitalPhaseLockedLoop(clocks_per_bit, clocks_per_bit / 5, 3));
	pll_->set_delegate(this);
}

void Controller::digital_phase_locked_loop_output_bit(int value) {
	process_input_bit(value, cycles_since_index_hole_);
}

#pragma mark - Drive actions

bool Controller::get_is_track_zero() {
	if(!drive_) return false;
	return drive_->get_is_track_zero();
}

bool Controller::get_drive_is_ready() {
	if(!drive_) return false;
	return drive_->has_disk();
}

bool Controller::get_drive_is_read_only() {
	if(!drive_) return false;
	return drive_->get_is_read_only();
}

void Controller::step(int direction) {
	invalidate_track();
	if(drive_) drive_->step(direction);
}

void Controller::set_motor_on(bool motor_on) {
	motor_is_on_ = motor_on;
}

bool Controller::get_motor_on() {
	return motor_is_on_;
}

void Controller::set_drive(std::shared_ptr<Drive> drive) {
	if(drive_ != drive)
	{
		invalidate_track();
		drive_ = drive;
	}
}

void Controller::invalidate_track() {
	track_ = nullptr;
	if(patched_track_) {
		drive_->set_track(patched_track_);
		patched_track_ = nullptr;
	}
}

void Controller::process_write_completed() {}
