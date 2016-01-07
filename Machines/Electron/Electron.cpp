//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

#include <algorithm>

using namespace Electron;

Machine::Machine()
{
	setup6502();
}

Machine::~Machine()
{
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	printf("%04x\n", address);

	if(address < 32768)
	{
		if(isReadOperation(operation))
		{
			*value = ram[address];
		}
		else
		{
			ram[address] = *value;
		}
	}
	else
	{
		if(address > 49152)
		{
			if(isReadOperation(operation)) *value = os[address - 49152];
		}
		else
		{
			if(isReadOperation(operation)) *value = basic[address - 32768];
		}
	}

	return 1;
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data)
{
	uint8_t *target = nullptr;
	switch(slot)
	{
		case ROMTypeBASIC:	target = basic;	break;
		case ROMTypeOS:		target = os;	break;
	}

	memcpy(target, data, std::min((size_t)16384, length));
}
