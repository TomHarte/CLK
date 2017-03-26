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
	: head_position_(0), head_(0) {}

void Drive::set_disk(const std::shared_ptr<Disk> &disk) {
	disk_ = disk;
	track_ = nullptr;
}

void Drive::set_disk_with_track(const std::shared_ptr<Track> &track) {
	disk_ = nullptr;
	track_ = track;
}

bool Drive::has_disk() {
	return (bool)disk_ || (bool)track_;
}

bool Drive::get_is_track_zero() {
	return head_position_ == 0;
}

void Drive::step(int direction) {
	head_position_ = std::max(head_position_ + direction, 0);
}

void Drive::set_head(unsigned int head) {
	head_ = head;
}

bool Drive::get_is_read_only() {
	if(disk_) return disk_->get_is_read_only();
	if(track_) return true;
	return false;
}

std::shared_ptr<Track> Drive::get_track() {
	if(disk_) return disk_->get_track_at_position(head_, (unsigned int)head_position_);
	if(track_) return track_;
	return nullptr;
}

void Drive::set_track(const std::shared_ptr<Track> &track) {
	if(disk_) disk_->set_track_at_position(head_, (unsigned int)head_position_, track);
}
