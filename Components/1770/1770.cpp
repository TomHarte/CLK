//
//  1770.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "1770.hpp"

using namespace WD;

void WD1770::set_drive(std::shared_ptr<Storage::Disk::Drive> drive)
{
}

void WD1770::set_is_double_density(bool is_double_density)
{
}

void WD1770::set_register(int address, uint8_t value)
{
}

uint8_t WD1770::get_register(int address)
{
	return 0;
}

void WD1770::run_for_cycles(unsigned int number_of_cycles)
{
}
