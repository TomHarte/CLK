//
//  1770.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "1770.hpp"

using namespace WD;

WD1770::WD1770() : state_(State::Waiting), status_(0), has_command_(false) {}

void WD1770::set_drive(std::shared_ptr<Storage::Disk::Drive> drive)
{
}

void WD1770::set_is_double_density(bool is_double_density)
{
}

void WD1770::set_register(int address, uint8_t value)
{
	switch(address&3)
	{
		case 0:
			command_ = value;
			has_command_ = true;
			// TODO: is this force interrupt?
		break;
		case 1:		track_ = value;		break;
		case 2:		sector_ = value;	break;
		case 3:		data_ = value;		break;
	}
}

uint8_t WD1770::get_register(int address)
{
	switch(address&3)
	{
		default:	return status_;
		case 1:		return track_;
		case 2:		return sector_;
		case 3:		return data_;
	}
}

void WD1770::run_for_cycles(unsigned int number_of_cycles)
{
	// perform one step every eight cycles, arbitrariy as I can find no timing documentation
	cycles += number_of_cycles;
	while(cycles > 8)
	{
		cycles -= 8;

		switch(state_)
		{
			case State::Waiting:
				if(has_command_)
				{
					has_command_ = false;
					if(command_ & 0x80)
						state_ = (command_ & 0x40) ? State::BeginType3 : State::BeginType2;
					else
						state_ = State::BeginType1;
				}
			continue;

			default:
				printf("Unhandled state %d!", state_);
			return;
		}
	}
}
