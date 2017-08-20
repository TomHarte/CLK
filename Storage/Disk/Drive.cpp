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

Drive::Drive()
	: head_position_(0), head_(0), has_disk_(false) {}

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
	return !has_disk_;
}

bool Drive::get_is_track_zero() {
	return head_position_ == 0;
}

void Drive::step(int direction) {
	head_position_ = std::max(head_position_ + direction, 0);
	printf("Head -> %d\n", head_position_);
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
