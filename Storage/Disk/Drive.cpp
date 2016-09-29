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
	: _head_position(0), _head(0) {}

void Drive::set_disk(std::shared_ptr<Disk> disk)
{
	_disk = disk;
}

bool Drive::has_disk()
{
	return (bool)_disk;
}

bool Drive::get_is_track_zero()
{
	return _head_position == 0;
}

void Drive::step(int direction)
{
	_head_position = std::max(_head_position + direction, 0);
}

void Drive::set_head(unsigned int head)
{
	_head = head;
}

std::shared_ptr<Track> Drive::get_track()
{
	if(_disk) return _disk->get_track_at_position(_head, (unsigned int)_head_position);
	return nullptr;
}
