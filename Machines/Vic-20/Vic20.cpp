//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

using namespace Vic20;

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	return 1;
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data)
{
}

void Machine::add_prg(size_t length, const uint8_t *data)
{
}
