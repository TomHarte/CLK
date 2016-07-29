//
//  DiskDrive.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "DiskDrive.hpp"

using namespace Storage;

DiskDrive::DiskDrive(unsigned int clock_rate, unsigned int revolutions_per_minute) :
	_clock_rate(clock_rate),
	_revolutions_per_minute(revolutions_per_minute),
	_head_position(0) {}

void DiskDrive::set_expected_bit_length(Time bit_length)
{
	_bit_length = bit_length;
}

void DiskDrive::set_disk(std::shared_ptr<Disk> disk)
{
	_disk = disk;
//	_track.reset();
}

bool DiskDrive::has_disk()
{
	return (bool)_disk;
}

bool DiskDrive::get_is_track_zero()
{
	return _head_position == 0;
}

void DiskDrive::step(int direction)
{
	_head_position = std::max(_head_position + direction, 0);
	_track = _disk->get_track_at_position((unsigned int)_head_position);
}

void DiskDrive::digital_phase_locked_loop_output_bit(int value)
{
	process_input_bit(value, _cycles_since_index_hole);
}

void DiskDrive::run_for_cycles(unsigned int number_of_cycles)
{
	if(has_disk())
	{
		while(number_of_cycles--)
		{

		}
	}
}
