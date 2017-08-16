//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"

using namespace Storage::Disk;

int Disk::get_id_for_track_at_position(unsigned int head, unsigned int position) {
	return (int)(position * get_head_count() + head);
}

void Disk::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	if(get_is_read_only()) return;

	int address = get_id_for_track_at_position(head, position);
	cached_tracks_[address] = track;

	if(!update_queue_) update_queue_.reset(new Concurrency::AsyncTaskQueue);
	std::shared_ptr<Track> track_copy(track->clone());
	update_queue_->enqueue([this, head, position, track_copy] {
		store_updated_track_at_position(head, position, track_copy, file_access_mutex_);
	});
}

std::shared_ptr<Track> Disk::get_track_at_position(unsigned int head, unsigned int position) {
	if(head >= get_head_count()) return nullptr;
	if(position >= get_head_position_count()) return nullptr;

	int address = get_id_for_track_at_position(head, position);
	std::map<int, std::shared_ptr<Track>>::iterator cached_track = cached_tracks_.find(address);
	if(cached_track != cached_tracks_.end()) return cached_track->second;

	std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
	std::shared_ptr<Track> track = get_uncached_track_at_position(head, position);
	cached_tracks_[address] = track;
	return track;
}

void Disk::store_updated_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track, std::mutex &file_access_mutex) {}

void Disk::flush_updates() {
	if(update_queue_) update_queue_->flush();
}
