//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"

using namespace Storage::Disk;

int Disk::get_id_for_track_at_position(unsigned int head, unsigned int position)
{
	return (int)(position * get_head_count() + head);
}

void Disk::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track)
{
	if(get_is_read_only()) return;

	int address = get_id_for_track_at_position(head, position);
	cached_tracks_[address] = track;
	modified_tracks_.insert(address);
}

std::shared_ptr<Track> Disk::get_track_at_position(unsigned int head, unsigned int position)
{
	int address = get_id_for_track_at_position(head, position);
	std::map<int, std::shared_ptr<Track>>::iterator cached_track = cached_tracks_.find(address);
	if(cached_track != cached_tracks_.end()) return cached_track->second;

	std::shared_ptr<Track> track = get_uncached_track_at_position(head, position);
	cached_tracks_[address] = track;
	return track;
}

std::shared_ptr<Track> Disk::get_modified_track_at_position(unsigned int head, unsigned int position)
{
	int address = get_id_for_track_at_position(head, position);
	if(modified_tracks_.find(address) == modified_tracks_.end()) return nullptr;
	std::map<int, std::shared_ptr<Track>>::iterator cached_track = cached_tracks_.find(address);
	if(cached_track == cached_tracks_.end()) return nullptr;
	return cached_track->second;
}

bool Disk::get_is_modified()
{
	return !modified_tracks_.empty();
}
