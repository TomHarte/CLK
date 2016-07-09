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
	_serialPortVIA->set_serial_port(_serialPort);
	_serialPortVIA->set_delegate(this);
	_driveVIA.set_delegate(this);
}

void Machine::set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus)
{
	_serialPort->set_serial_bus(serial_bus);
	serial_bus->add_port(_serialPort);
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
//	if(operation == CPU6502::BusOperation::ReadOpcode && (address >= 0xE9C9 && address <= 0xEA2D)) printf("%04x\n", address);
//	if(operation == CPU6502::BusOperation::ReadOpcode && (address == 0xE887)) printf("A: %02x\n", get_value_of_register(CPU6502::Register::A));

/*	static bool log = false;

	if(operation == CPU6502::BusOperation::ReadOpcode)
	{
		log = (address >= 0xE85B && address <= 0xE907) || (address >= 0xE9C9 && address <= 0xEA2D);
		 if(log) printf("\n%04x: ", address);
	}
	if(log)  printf("[%c %04x] ", isReadOperation(operation) ? 'r' : 'w', address);*/

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
		if(address == 0x1805)
		{
			printf("Timer\n");
		}
		if(isReadOperation(operation))
			*value = _serialPortVIA->get_register(address);
		else
			_serialPortVIA->set_register(address, *value);
	}
	else if(address >= 0x1c00 && address <= 0x1c0f)
	{
		if(isReadOperation(operation))
			*value = _driveVIA.get_register(address);
		else
			_driveVIA.set_register(address, *value);
	}

	_serialPortVIA->run_for_half_cycles(2);
	_driveVIA.run_for_half_cycles(2);

	return 1;
}

void Machine::set_rom(const uint8_t *rom)
{
	memcpy(_rom, rom, sizeof(_rom));
}

#pragma mark - 6522 delegate

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
	set_irq_line(_serialPortVIA->get_interrupt_line() || _driveVIA.get_interrupt_line());
}
