//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

#include <algorithm>

using namespace Vic20;

Machine::Machine()
{}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	return 1;
}

#pragma mark - Setup

void Machine::setup_output(float aspect_ratio)
{
	_mos6560 = std::unique_ptr<MOS::MOS6560>(new MOS::MOS6560());
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data)
{
	uint8_t *target = nullptr;
	size_t max_length = 0x2000;
	switch(slot)
	{
		case ROMSlotKernel:		target = _kernelROM;							break;
		case ROMSlotCharacters:	target = _characterROM;	max_length = 0x1000;	break;
		case ROMSlotBASIC:		target = _basicROM;								break;
	}

	if(target)
	{
		size_t length_to_copy = std::min(max_length, length);
		memcpy(target, data, length_to_copy);
	}
}

void Machine::add_prg(size_t length, const uint8_t *data)
{
}
