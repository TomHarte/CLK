//
//  Drive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Drive.hpp"

#include "UnformattedTrack.hpp"

#include <algorithm>
#include <cassert>

using namespace Storage::Disk;

Drive::Drive(unsigned int input_clock_rate, int revolutions_per_minute, int number_of_heads):
	Storage::TimedEventLoop(input_clock_rate),
	rotational_multiplier_(60, revolutions_per_minute),
	available_heads_(number_of_heads) {
}

Drive::~Drive() {
	if(disk_) disk_->flush_tracks();
}

void Drive::set_disk(const std::shared_ptr<Disk> &disk) {
	if(disk_) disk_->flush_tracks();
	disk_ = disk;
	has_disk_ = !!disk_;

	invalidate_track();
	update_sleep_observer();
}

bool Drive::has_disk() {
	return has_disk_;
}

bool Drive::is_sleeping() {
	return !motor_is_on_ || !has_disk_;
}

bool Drive::get_is_track_zero() {
	return head_position_ == 0;
}

void Drive::step(int direction) {
	int old_head_position = head_position_;
	head_position_ = std::max(head_position_ + direction, 0);

	// If the head moved, flush the old track.
	if(head_position_ != old_head_position) {
		track_ = nullptr;
	}
}

void Drive::set_head(int head) {
	head = std::min(head, available_heads_ - 1);
	if(head != head_) {
		head_ = head;
		track_ = nullptr;
	}
}

Storage::Time Drive::get_time_into_track() {
	// `result` will initially be amount of time since the index hole was seen as a
	// proportion of a second; convert it into proportion of a rotation, simplify and return.
	Time result(cycles_since_index_hole_, static_cast<int>(get_input_clock_rate()));
	result /= rotational_multiplier_;
	result.simplify();
	assert(result <= Time(1));
	return result;
}

bool Drive::get_is_read_only() {
	if(disk_) return disk_->get_is_read_only();
	return true;
}

bool Drive::get_is_ready() {
	return true;
	return ready_index_count_ == 2;
}

void Drive::set_motor_on(bool motor_is_on) {
	motor_is_on_ = motor_is_on;
	if(!motor_is_on) {
		ready_index_count_ = 0;
		if(disk_) disk_->flush_tracks();
	}
	update_sleep_observer();
}

bool Drive::get_motor_on() {
	return motor_is_on_;
}

void Drive::set_event_delegate(Storage::Disk::Drive::EventDelegate *delegate) {
	event_delegate_ = delegate;
}

void Drive::advance(const Cycles cycles) {
	cycles_since_index_hole_ += static_cast<unsigned int>(cycles.as_int());
	if(event_delegate_) event_delegate_->advance(cycles);
}

void Drive::run_for(const Cycles cycles) {
	if(has_disk_ && motor_is_on_) {
		Time zero(0);

		int number_of_cycles = cycles.as_int();
		while(number_of_cycles) {
			int cycles_until_next_event = static_cast<int>(get_cycles_until_next_event());
			int cycles_to_run_for = std::min(cycles_until_next_event, number_of_cycles);
			if(!is_reading_ && cycles_until_bits_written_ > zero) {
				int write_cycles_target = static_cast<int>(cycles_until_bits_written_.get_unsigned_int());
				if(cycles_until_bits_written_.length % cycles_until_bits_written_.clock_rate) write_cycles_target++;
				cycles_to_run_for = std::min(cycles_to_run_for, write_cycles_target);
			}

			number_of_cycles -= cycles_to_run_for;
			if(!is_reading_) {
				if(cycles_until_bits_written_ > zero) {
					Storage::Time cycles_to_run_for_time(cycles_to_run_for);
					if(cycles_until_bits_written_ <= cycles_to_run_for_time) {
						if(event_delegate_) event_delegate_->process_write_completed();
						if(cycles_until_bits_written_ <= cycles_to_run_for_time)
							cycles_until_bits_written_.set_zero();
						else
							cycles_until_bits_written_ -= cycles_to_run_for_time;
					} else {
						cycles_until_bits_written_ -= cycles_to_run_for_time;
					}
				}
			}
			TimedEventLoop::run_for(Cycles(cycles_to_run_for));
		}
	}
}

#pragma mark - Track timed event loop

void Drive::get_next_event(const Time &duration_already_passed) {
	// Grab a new track if not already in possession of one. This will recursively call get_next_event,
	// supplying a proper duration_already_passed.
	if(!track_) {
		setup_track();
		return;
	}

	if(track_) {
		current_event_ = track_->get_next_event();
	} else {
		current_event_.length.length = 1;
		current_event_.length.clock_rate = 1;
		current_event_.type = Track::Event::IndexHole;
	}

	// divide interval, which is in terms of a single rotation of the disk, by rotation speed to
	// convert it into revolutions per second; this is achieved by multiplying by rotational_multiplier_
	assert(current_event_.length <= Time(1) && current_event_.length >= Time(0));
	Time interval = (current_event_.length - duration_already_passed) * rotational_multiplier_;
	set_next_event_time_interval(interval);
}

void Drive::process_next_event() {
	// TODO: ready test here.
	if(current_event_.type == Track::Event::IndexHole) {
		assert(get_time_into_track() == Time(1) || get_time_into_track() == Time(0));
		if(ready_index_count_ < 2) ready_index_count_++;
		cycles_since_index_hole_ = 0;
	}
	if(
		event_delegate_ &&
		(current_event_.type == Track::Event::IndexHole || is_reading_)
	){
		event_delegate_->process_event(current_event_);
	}
	get_next_event(Time(0));
}

#pragma mark - Track management

std::shared_ptr<Track> Drive::get_track() {
	if(disk_) return disk_->get_track_at_position(Track::Address(head_, head_position_));
	return nullptr;
}

void Drive::set_track(const std::shared_ptr<Track> &track) {
	if(disk_) disk_->set_track_at_position(Track::Address(head_, head_position_), track);
}

void Drive::setup_track() {
	track_ = get_track();
	if(!track_) {
		track_.reset(new UnformattedTrack);
	}

	Time offset;
	Time track_time_now = get_time_into_track();
	assert(track_time_now >= Time(0) && current_event_.length <= Time(1));

	Time time_found = track_->seek_to(track_time_now);
	assert(time_found >= Time(0) && time_found < Time(1) && time_found <= track_time_now);

	offset = track_time_now - time_found;
	get_next_event(offset);
}

void Drive::invalidate_track() {
	track_ = nullptr;
	if(patched_track_) {
		set_track(patched_track_);
		patched_track_ = nullptr;
	}
}

#pragma mark - Writing

void Drive::begin_writing(Time bit_length, bool clamp_to_index_hole) {
	is_reading_ = false;
	clamp_writing_to_index_hole_ = clamp_to_index_hole;

	cycles_per_bit_ = Storage::Time(get_input_clock_rate()) * bit_length;
	cycles_per_bit_.simplify();

	write_segment_.length_of_a_bit = bit_length / rotational_multiplier_;
	write_segment_.data.clear();
	write_segment_.number_of_bits = 0;

	write_start_time_ = get_time_into_track();
}

void Drive::write_bit(bool value) {
	bool needs_new_byte = !(write_segment_.number_of_bits&7);
	if(needs_new_byte) write_segment_.data.push_back(0);
	if(value) write_segment_.data[write_segment_.number_of_bits >> 3] |= 0x80 >> (write_segment_.number_of_bits & 7);
	write_segment_.number_of_bits++;

	cycles_until_bits_written_ += cycles_per_bit_;
}

void Drive::end_writing() {
	if(!is_reading_) {
		is_reading_ = true;

		if(!patched_track_) {
			// Avoid creating a new patched track if this one is already patched
			patched_track_ = std::dynamic_pointer_cast<PCMPatchedTrack>(track_);
			if(!patched_track_) {
				patched_track_.reset(new PCMPatchedTrack(track_));
			}
		}
		patched_track_->add_segment(write_start_time_, write_segment_, clamp_writing_to_index_hole_);
		cycles_since_index_hole_ %= get_input_clock_rate();
		invalidate_track();
	}
}
