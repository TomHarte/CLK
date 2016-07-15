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
	_revolutions_per_minute(revolutions_per_minute) {}

void DiskDrive::set_expected_bit_length(Time bit_length)
{
	_bit_length = bit_length;
}
