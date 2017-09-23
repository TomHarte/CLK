//
//  DiskImage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "DiskImage.hpp"

using namespace Storage::Disk;

int DiskImageHolderBase::get_id_for_track_at_position(unsigned int head, unsigned int position) {
	return (int)(position * get_head_count() + head);
}
