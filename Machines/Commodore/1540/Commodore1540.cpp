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

Machine::Machine()
{
	_serialPortVIA.reset(new SerialPortVIA);
	_serialPort.reset(new SerialPort);
	_serialPort->set_serial_port_via(_serialPortVIA);
}

void Machine::set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus)
{
	_serialPort->set_serial_bus(serial_bus);
	serial_bus->add_port(_serialPort);
}

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
	else if(address >= 0x1800 && address <= 0x180f)
	{
		if(isReadOperation(operation))
			*value = _serialPortVIA->get_register(address);
		else
			_serialPortVIA->set_register(address, *value);
	}

	_serialPortVIA->run_for_half_cycles(2);

	return 1;
}

void Machine::set_rom(const uint8_t *rom)
{
	memcpy(_rom, rom, sizeof(_rom));
}
