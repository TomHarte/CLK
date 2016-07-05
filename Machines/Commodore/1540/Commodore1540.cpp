//
//  Commodore1540.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Commodore1540.hpp"
#include <string.h>

using namespace Commodore::C1540;

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	if(address < 0x800)
	{
		if(isReadOperation(operation))
			*value = _ram[address];
		else
			_ram[address] = *value;
	}
	else if(address >= 0xc000)
	{
		if(isReadOperation(operation))
			*value = _rom[address & 0x3fff];
	}

	return 1;
}

void Machine::set_rom(uint8_t *rom)
{
	memcpy(_rom, rom, sizeof(_rom));
}
