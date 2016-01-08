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
			if((address & 0xff00) == 0xfe00)
			{
				printf("%c: %02x: ", isReadOperation(operation) ? 'r' : 'w', *value);

				switch(address&0xf)
				{
					case 0x0:
						printf("Interrupt status or control\n");
					break;
					case 0x1:
					break;
					case 0x2:
					case 0x3:
						printf("Screen start address\n");
					break;
					case 0x4:
						printf("Cassette\n");
					break;
					case 0x5:
						printf("Interrupt clear and paging\n");
					break;
					case 0x6:
						printf("Counter\n");
					break;
					case 0x7:
						printf("Misc. control\n");
					break;
					default:
						printf("Palette\n");
					break;
				}
			}
			else
			{
				if(isReadOperation(operation))
					*value = os[address - 49152];
			}
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
