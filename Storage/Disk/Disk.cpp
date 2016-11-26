//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"

using namespace Storage::Disk;

std::shared_ptr<Track> Disk::get_track_at_position(unsigned int head, unsigned int position)
{
	int address = (int)(position * get_head_count() + head);
	std::map<int, std::shared_ptr<Track>>::iterator cached_track = cached_tracks_.find(address);
	if(cached_track != cached_tracks_.end()) return cached_track->second;

	std::shared_ptr<Track> track = virtual_get_track_at_position(head, position);
	cached_tracks_[address] = track;
	return track;
}
