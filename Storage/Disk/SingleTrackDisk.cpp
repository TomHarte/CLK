//
//  SingleTrackDisk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "SingleTrackDisk.hpp"

using namespace Storage::Disk;

SingleTrackDisk::SingleTrackDisk(const std::shared_ptr<Track> &track) :
	track_(track) {}

unsigned int SingleTrackDisk::get_head_position_count() {
	return 1;
}

std::shared_ptr<Track> SingleTrackDisk::get_uncached_track_at_position(unsigned int head, unsigned int position) {
	return track_;
}
