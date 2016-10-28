//
//  Commodore1540.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "C1540.hpp"
#include <string>
#include "../../../Storage/Disk/Encodings/CommodoreGCR.hpp"

using namespace Commodore::C1540;

Machine::Machine() :
	_shift_register(0),
	Storage::Disk::Controller(1000000, 4, 300)
{
	// create a serial port and a VIA to run it
	_serialPortVIA.reset(new SerialPortVIA);
	_serialPort.reset(new SerialPort);

	// attach the serial port to its VIA and vice versa
	_serialPort->set_serial_port_via(_serialPortVIA);
	_serialPortVIA->set_serial_port(_serialPort);

	// set this instance as the delegate to receive interrupt requests from both VIAs
	_serialPortVIA->set_interrupt_delegate(this);
	_driveVIA.set_interrupt_delegate(this);
	_driveVIA.set_delegate(this);

	// set a bit rate
	set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(3));
}

void Machine::set_serial_bus(std::shared_ptr<::Commodore::Serial::Bus> serial_bus)
{
	Commodore::Serial::AttachPortAndBus(_serialPort, serial_bus);
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
//	static bool log = false;
//	if(operation == CPU6502::BusOperation::ReadOpcode && (address == 0xF3C0)) log = true;
//	if(operation == CPU6502::BusOperation::ReadOpcode && log) printf("%04x\n", address);
//	if(operation == CPU6502::BusOperation::ReadOpcode) printf("%04x\n", address);
//	if(operation == CPU6502::BusOperation::ReadOpcode && (address >= 0xF510 && address <= 0xF553)) printf("%04x\n", address);
//	if(operation == CPU6502::BusOperation::ReadOpcode && (address == 0xE887)) printf("A: %02x\n", get_value_of_register(CPU6502::Register::A));

/*	static bool log = false;

	if(operation == CPU6502::BusOperation::ReadOpcode)
	{
		log = (address >= 0xE85B && address <= 0xE907) || (address >= 0xE9C9 && address <= 0xEA2D);
		 if(log) printf("\n%04x: ", address);
	}
	if(log)  printf("[%c %04x] ", isReadOperation(operation) ? 'r' : 'w', address);*/

	/*
		Memory map (given that I'm unsure yet on any potential mirroring):

			0x0000–0x07ff	RAM
			0x1800–0x180f	the serial-port VIA
			0x1c00–0x1c0f	the drive VIA
			0xc000–0xffff	ROM
	*/
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
	else if(address >= 0x1c00 && address <= 0x1c0f)
	{
		if(isReadOperation(operation))
			*value = _driveVIA.get_register(address);
		else
			_driveVIA.set_register(address, *value);
	}

	_serialPortVIA->run_for_cycles(1);
	_driveVIA.run_for_cycles(1);

	return 1;
}

void Machine::set_rom(const uint8_t *rom)
{
	memcpy(_rom, rom, sizeof(_rom));
}

void Machine::set_disk(std::shared_ptr<Storage::Disk::Disk> disk)
{
	std::shared_ptr<Storage::Disk::Drive> drive(new Storage::Disk::Drive);
	drive->set_disk(disk);
	set_drive(drive);
}

void Machine::run_for_cycles(int number_of_cycles)
{
	CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles);
	set_motor_on(_driveVIA.get_motor_enabled());
	if(_driveVIA.get_motor_enabled()) // TODO: motor speed up/down
		Storage::Disk::Controller::run_for_cycles(number_of_cycles);
}

#pragma mark - 6522 delegate

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
	// both VIAs are connected to the IRQ line
	set_irq_line(_serialPortVIA->get_interrupt_line() || _driveVIA.get_interrupt_line());
}

#pragma mark - Disk drive

void Machine::process_input_bit(int value, unsigned int cycles_since_index_hole)
{
	_shift_register = (_shift_register << 1) | value;
	if((_shift_register & 0x3ff) == 0x3ff)
	{
		_driveVIA.set_sync_detected(true);
		_bit_window_offset = -1; // i.e. this bit isn't the first within a data window, but the next might be
	}
	else
	{
		_driveVIA.set_sync_detected(false);
	}
	_bit_window_offset++;
	if(_bit_window_offset == 8)
	{
		_driveVIA.set_data_input((uint8_t)_shift_register);
		_bit_window_offset = 0;
		if(_driveVIA.get_should_set_overflow())
		{
			set_overflow_line(true);
		}
	}
	else
		set_overflow_line(false);
}

// the 1540 does not recognise index holes
void Machine::process_index_hole()	{}

#pragma mak - Drive VIA delegate

void Machine::drive_via_did_step_head(void *driveVIA, int direction)
{
	step(direction);
}

void Machine::drive_via_did_set_data_density(void *driveVIA, int density)
{
	set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone((unsigned int)density));
}

#pragma mark - SerialPortVIA

SerialPortVIA::SerialPortVIA() :
	_portB(0x00), _attention_acknowledge_level(false), _attention_level_input(true), _data_level_output(false)
{}

uint8_t SerialPortVIA::get_port_input(Port port)
{
	if(port) return _portB;
	return 0xff;
}

void SerialPortVIA::set_port_output(Port port, uint8_t value, uint8_t mask)
{
	if(port)
	{
		std::shared_ptr<::Commodore::Serial::Port> serialPort = _serialPort.lock();
		if(serialPort) {
			_attention_acknowledge_level = !(value&0x10);
			_data_level_output = (value&0x02);

			serialPort->set_output(::Commodore::Serial::Line::Clock, (::Commodore::Serial::LineLevel)!(value&0x08));
			update_data_line();
		}
	}
}

void SerialPortVIA::set_serial_line_state(::Commodore::Serial::Line line, bool value)
{
	switch(line)
	{
		default: break;
		case ::Commodore::Serial::Line::Data:		_portB = (_portB & ~0x01) | (value ? 0x00 : 0x01);		break;
		case ::Commodore::Serial::Line::Clock:		_portB = (_portB & ~0x04) | (value ? 0x00 : 0x04);		break;
		case ::Commodore::Serial::Line::Attention:
			_attention_level_input = !value;
			_portB = (_portB & ~0x80) | (value ? 0x00 : 0x80);
			set_control_line_input(Port::A, Line::One, !value);
			update_data_line();
		break;
	}
}

void SerialPortVIA::set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort)
{
	_serialPort = serialPort;
}

void SerialPortVIA::update_data_line()
{
	std::shared_ptr<::Commodore::Serial::Port> serialPort = _serialPort.lock();
	if(serialPort)
	{
		// "ATN (Attention) is an input on pin 3 of P2 and P3 that is sensed at PB7 and CA1 of UC3 after being inverted by UA1"
		serialPort->set_output(::Commodore::Serial::Line::Data,
			(::Commodore::Serial::LineLevel)(!_data_level_output
			&& (_attention_level_input != _attention_acknowledge_level)));
	}
}

#pragma mark - DriveVIA

void DriveVIA::set_delegate(Delegate *delegate)
{
	_delegate = delegate;
}

// write protect tab uncovered
DriveVIA::DriveVIA() : _port_b(0xff), _port_a(0xff), _delegate(nullptr) {}

uint8_t DriveVIA::get_port_input(Port port) {
	return port ? _port_b : _port_a;
}

void DriveVIA::set_sync_detected(bool sync_detected) {
	_port_b = (_port_b & 0x7f) | (sync_detected ? 0x00 : 0x80);
}

void DriveVIA::set_data_input(uint8_t value) {
	_port_a = value;
}

bool DriveVIA::get_should_set_overflow() {
	return _should_set_overflow;
}

bool DriveVIA::get_motor_enabled() {
	return _drive_motor;
}

void DriveVIA::set_control_line_output(Port port, Line line, bool value) {
	if(port == Port::A && line == Line::Two) {
		_should_set_overflow = value;
	}
}

void DriveVIA::set_port_output(Port port, uint8_t value, uint8_t direction_mask) {
	if(port)
	{
		// record drive motor state
		_drive_motor = !!(value&4);

		// check for a head step
		int step_difference = ((value&3) - (_previous_port_b_output&3))&3;
		if(step_difference)
		{
			if(_delegate) _delegate->drive_via_did_step_head(this, (step_difference == 1) ? 1 : -1);
		}

		// check for a change in density
		int density_difference = (_previous_port_b_output^value) & (3 << 5);
		if(density_difference && _delegate)
		{
			_delegate->drive_via_did_set_data_density(this, (value >> 5)&3);
		}

		// TODO: something with the drive LED
//		printf("LED: %s\n", value&8 ? "On" : "Off");

		_previous_port_b_output = value;
	}
}

#pragma mark - SerialPort

void SerialPort::set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level) {
	std::shared_ptr<SerialPortVIA> serialPortVIA = _serialPortVIA.lock();
	if(serialPortVIA) serialPortVIA->set_serial_line_state(line, (bool)level);
}

void SerialPort::set_serial_port_via(std::shared_ptr<SerialPortVIA> serialPortVIA) {
	_serialPortVIA = serialPortVIA;
}
