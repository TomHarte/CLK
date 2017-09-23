//
//  SingleTrackDisk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "SingleTrackDisk.hpp"

using namespace Storage::Disk;

SingleTrackDiskImage::SingleTrackDiskImage(const std::shared_ptr<Track> &track) :
	track_(track) {}

unsigned int SingleTrackDiskImage::get_head_position_count() {
	return 1;
}

std::shared_ptr<Track> SingleTrackDiskImage::get_track_at_position(unsigned int head, unsigned int position) {
	return track_;
}
