//
//  Drive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Drive.hpp"
#include <algorithm>

using namespace Storage::Disk;

Drive::Drive(unsigned int input_clock_rate, int revolutions_per_minute):
	Storage::TimedEventLoop(input_clock_rate),
	rotational_multiplier_(60, revolutions_per_minute) {
}

void Drive::set_disk(const std::shared_ptr<Disk> &disk) {
	disk_ = disk;
	track_ = nullptr;
	has_disk_ = !!disk_;
	update_sleep_observer();
}

void Drive::set_disk_with_track(const std::shared_ptr<Track> &track) {
	disk_ = nullptr;
	track_ = track;
	has_disk_ = !!track_;
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

	// If the head moved and this drive has a real disk in it, flush the old track.
	if(head_position_ != old_head_position && disk_ != nullptr) {
		track_ = nullptr;
	}
}

Storage::Time Drive::get_time_into_track() {
	// `result` will initially be amount of time since the index hole was seen as a
	// proportion of a second; convert it into proportion of a rotation, simplify and return.
	Time result(cycles_since_index_hole_, 8000000);
	result /= rotational_multiplier_;
	result.simplify();
	return result;
}

void Drive::set_head(unsigned int head) {
	head_ = head;
}

bool Drive::get_is_read_only() {
	if(disk_) return disk_->get_is_read_only();
	return true;
}

bool Drive::get_is_ready() {
	// TODO: a real test for this.
	return disk_ != nullptr;
}

std::shared_ptr<Track> Drive::get_track() {
	if(disk_) return disk_->get_track_at_position(head_, (unsigned int)head_position_);
	if(track_) return track_;
	return nullptr;
}

void Drive::set_track(const std::shared_ptr<Track> &track) {
	if(disk_) disk_->set_track_at_position(head_, (unsigned int)head_position_, track);
}

void Drive::set_motor_on(bool motor_is_on) {
	motor_is_on_ = motor_is_on;
	update_sleep_observer();
}

void Drive::process_next_event() {
	if(event_delegate_) event_delegate_->process_event(current_event_);
}

void Drive::run_for(const Cycles cycles) {
}
