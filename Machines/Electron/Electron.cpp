//
//  Electron.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Electron.hpp"

using namespace Electron;

Machine::Machine()
{
}

Machine::~Machine()
{
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	return 1;
}
