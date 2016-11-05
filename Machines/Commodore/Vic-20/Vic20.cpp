//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

#include <algorithm>
#include "../../../Storage/Tape/Formats/TapePRG.hpp"
#include "../../../StaticAnalyser/StaticAnalyser.hpp"

using namespace Commodore::Vic20;

Machine::Machine() :
	_rom(nullptr),
	_is_running_at_zero_cost(false),
	_tape(1022727)
{
	// create 6522s, serial port and bus
	_userPortVIA.reset(new UserPortVIA);
	_keyboardVIA.reset(new KeyboardVIA);
	_serialPort.reset(new SerialPort);
	_serialBus.reset(new ::Commodore::Serial::Bus);

	// wire up the serial bus and serial port
	Commodore::Serial::AttachPortAndBus(_serialPort, _serialBus);

	// wire up 6522s and serial port
	_userPortVIA->set_serial_port(_serialPort);
	_keyboardVIA->set_serial_port(_serialPort);
	_serialPort->set_user_port_via(_userPortVIA);

	// wire up the 6522s, tape and machine
	_userPortVIA->set_interrupt_delegate(this);
	_keyboardVIA->set_interrupt_delegate(this);
	_tape.set_delegate(this);

	// establish the memory maps
	set_memory_size(MemorySize::Default);

	// set the NTSC clock rate
	set_region(NTSC);
//	_debugPort.reset(new ::Commodore::Serial::DebugPort);
//	_debugPort->set_serial_bus(_serialBus);
//	_serialBus->add_port(_debugPort);
}

void Machine::set_memory_size(MemorySize size)
{
	memset(_processorReadMemoryMap, 0, sizeof(_processorReadMemoryMap));
	memset(_processorWriteMemoryMap, 0, sizeof(_processorWriteMemoryMap));

	switch(size)
	{
		default: break;
		case ThreeKB:
			write_to_map(_processorReadMemoryMap, _expansionRAM, 0x0000, 0x1000);
			write_to_map(_processorWriteMemoryMap, _expansionRAM, 0x0000, 0x1000);
		break;
		case ThirtyTwoKB:
			write_to_map(_processorReadMemoryMap, _expansionRAM, 0x0000, 0x8000);
			write_to_map(_processorWriteMemoryMap, _expansionRAM, 0x0000, 0x8000);
		break;
	}

	// install the system ROMs and VIC-visible memory
	write_to_map(_processorReadMemoryMap, _userBASICMemory, 0x0000, sizeof(_userBASICMemory));
	write_to_map(_processorReadMemoryMap, _screenMemory, 0x1000, sizeof(_screenMemory));
	write_to_map(_processorReadMemoryMap, _colorMemory, 0x9400, sizeof(_colorMemory));
	write_to_map(_processorReadMemoryMap, _characterROM, 0x8000, sizeof(_characterROM));
	write_to_map(_processorReadMemoryMap, _basicROM, 0xc000, sizeof(_basicROM));
	write_to_map(_processorReadMemoryMap, _kernelROM, 0xe000, sizeof(_kernelROM));

	write_to_map(_processorWriteMemoryMap, _userBASICMemory, 0x0000, sizeof(_userBASICMemory));
	write_to_map(_processorWriteMemoryMap, _screenMemory, 0x1000, sizeof(_screenMemory));
	write_to_map(_processorWriteMemoryMap, _colorMemory, 0x9400, sizeof(_colorMemory));

	// install the inserted ROM if there is one
	if(_rom)
	{
		write_to_map(_processorReadMemoryMap, _rom, _rom_address, _rom_length);
	}
}

void Machine::write_to_map(uint8_t **map, uint8_t *area, uint16_t address, uint16_t length)
{
	address >>= 10;
	length >>= 10;
	while(length--)
	{
		map[address] = area;
		area += 0x400;
		address++;
	}
}

Machine::~Machine()
{
	delete[] _rom;
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
//	static int logCount = 0;
//	if(operation == CPU6502::BusOperation::ReadOpcode && address == 0xf957) logCount = 500;
//	if(operation == CPU6502::BusOperation::ReadOpcode && logCount) {
//		logCount--;
//		printf("%04x\n", address);
//	}

//	if(operation == CPU6502::BusOperation::Write && (address >= 0x033C && address < 0x033C + 192))
//	{
//		printf("\n[%04x] <- %02x\n", address, *value);
//	}

	// run the phase-1 part of this cycle, in which the VIC accesses memory
	if(!_is_running_at_zero_cost) _mos6560->run_for_cycles(1);

	// run the phase-2 part of the cycle, which is whatever the 6502 said it should be
	if(isReadOperation(operation))
	{
		uint8_t result = _processorReadMemoryMap[address >> 10] ? _processorReadMemoryMap[address >> 10][address & 0x3ff] : 0xff;
		if((address&0xfc00) == 0x9000)
		{
			if((address&0xff00) == 0x9000)	result &= _mos6560->get_register(address);
			if((address&0xfc10) == 0x9010)	result &= _userPortVIA->get_register(address);
			if((address&0xfc20) == 0x9020)	result &= _keyboardVIA->get_register(address);
		}
		*value = result;

		// This combined with the stuff below constitutes the fast tape hack. Performed here: if the
		// PC hits the start of the loop that just waits for an interesting tape interrupt to have
		// occurred then skip both 6522s and the tape ahead to the next interrupt without any further
		// CPU or 6560 costs.
		if(_use_fast_tape_hack && _tape.has_tape() && address == 0xf92f && operation == CPU6502::BusOperation::ReadOpcode)
		{
			while(!_userPortVIA->get_interrupt_line() && !_keyboardVIA->get_interrupt_line() && !_tape.get_tape()->is_at_end())
			{
				_userPortVIA->run_for_cycles(1);
				_keyboardVIA->run_for_cycles(1);
				_tape.run_for_cycles(1);
			}
		}
	}
	else
	{
		uint8_t *ram = _processorWriteMemoryMap[address >> 10];
		if(ram) ram[address & 0x3ff] = *value;
		if((address&0xfc00) == 0x9000)
		{
			if((address&0xff00) == 0x9000)	_mos6560->set_register(address, *value);
			if((address&0xfc10) == 0x9010)	_userPortVIA->set_register(address, *value);
			if((address&0xfc20) == 0x9020)	_keyboardVIA->set_register(address, *value);
		}
	}

	_userPortVIA->run_for_cycles(1);
	_keyboardVIA->run_for_cycles(1);
	if(_typer && operation == CPU6502::BusOperation::ReadOpcode && address == 0xEB1E)
	{
		if(!_typer->type_next_character())
		{
			clear_all_keys();
			_typer.reset();
		}
	}
	_tape.run_for_cycles(1);
	if(_c1540) _c1540->run_for_cycles(1);

	// If using fast tape then:
	//	if the PC hits 0xf98e, the ROM's tape loading routine, then begin zero cost processing;
	//	if the PC heads into RAM
	//
	// Where 'zero cost processing' is taken to be taking the 6560 off the bus (because I know it's
	// expensive, and not relevant) then running the tape, the CPU and both 6522s as usual but not
	// counting cycles towards the processing budget. So the limit is the host machine.
	//
	// Note the additional test above for PC hitting 0xf92f, which is a loop in the ROM that waits
	// for an interesting interrupt. Up there the fast tape hack goes even further in also cutting
	// the CPU out of the action.
	if(_use_fast_tape_hack && _tape.has_tape())
	{
		if(address == 0xf98e && operation == CPU6502::BusOperation::ReadOpcode)
		{
			_is_running_at_zero_cost = true;
			set_clock_is_unlimited(true);
		}
		if(
			(address < 0xe000 && operation == CPU6502::BusOperation::ReadOpcode) ||
			_tape.get_tape()->is_at_end()
		)
		{
			_is_running_at_zero_cost = false;
			set_clock_is_unlimited(false);
		}
	}

	return 1;
}

#pragma mark - 6522 delegate

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
	set_nmi_line(_userPortVIA->get_interrupt_line());
	set_irq_line(_keyboardVIA->get_interrupt_line());
}

#pragma mark - Setup

void Machine::set_region(Commodore::Vic20::Region region)
{
	_region = region;
	switch(region)
	{
		case PAL:
			set_clock_rate(1108404);
			if(_mos6560)
			{
				_mos6560->set_output_mode(MOS::MOS6560<Commodore::Vic20::Vic6560>::OutputMode::PAL);
				_mos6560->set_clock_rate(1108404);
			}
		break;
		case NTSC:
			set_clock_rate(1022727);
			if(_mos6560)
			{
				_mos6560->set_output_mode(MOS::MOS6560<Commodore::Vic20::Vic6560>::OutputMode::NTSC);
				_mos6560->set_clock_rate(1022727);
			}
		break;
	}
}

void Machine::setup_output(float aspect_ratio)
{
	_mos6560.reset(new Vic6560());
	_mos6560->get_speaker()->set_high_frequency_cut_off(1600);	// There is a 1.6Khz low-pass filter in the Vic-20.
	set_region(_region);

	memset(_mos6560->_videoMemoryMap, 0, sizeof(_mos6560->_videoMemoryMap));
	write_to_map(_mos6560->_videoMemoryMap, _characterROM, 0x0000, sizeof(_characterROM));
	write_to_map(_mos6560->_videoMemoryMap, _userBASICMemory, 0x2000, sizeof(_userBASICMemory));
	write_to_map(_mos6560->_videoMemoryMap, _screenMemory, 0x3000, sizeof(_screenMemory));
	_mos6560->_colorMemory = _colorMemory;
}

void Machine::close_output()
{
	_mos6560 = nullptr;
}

void Machine::set_rom(ROMSlot slot, size_t length, const uint8_t *data)
{
	uint8_t *target = nullptr;
	size_t max_length = 0x2000;
	switch(slot)
	{
		case Kernel:		target = _kernelROM;							break;
		case Characters:	target = _characterROM;	max_length = 0x1000;	break;
		case BASIC:			target = _basicROM;								break;
		case Drive:
			_driveROM.reset(new uint8_t[length]);
			memcpy(_driveROM.get(), data, length);
			install_disk_rom();
		return;
	}

	if(target)
	{
		size_t length_to_copy = std::min(max_length, length);
		memcpy(target, data, length_to_copy);
	}
}

//void Machine::set_prg(const char *file_name, size_t length, const uint8_t *data)
//{
//	if(length > 2)
//	{
//		_rom_address = (uint16_t)(data[0] | (data[1] << 8));
//		_rom_length = (uint16_t)(length - 2);
//
//		// install in the ROM area if this looks like a ROM; otherwise put on tape and throw into that mechanism
//		if(_rom_address == 0xa000)
//		{
//			_rom = new uint8_t[0x2000];
//			memcpy(_rom, &data[2], length - 2);
//			write_to_map(_processorReadMemoryMap, _rom, _rom_address, 0x2000);
//		}
//		else
//		{
//			set_tape(std::shared_ptr<Storage::Tape::Tape>(new Storage::Tape::PRG(file_name)));
//		}
//	}
//}

#pragma mar - Tape

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
	if(target.tapes.size())
	{
		_tape.set_tape(target.tapes.front());
	}

	if(target.disks.size())
	{
		// construct the 1540
		_c1540.reset(new ::Commodore::C1540::Machine);

		// attach it to the serial bus
		_c1540->set_serial_bus(_serialBus);

		// hand it the disk
		_c1540->set_disk(target.disks.front());

		// install the ROM if it was previously set
		install_disk_rom();
	}

	if(target.cartridges.size())
	{
		_rom_address = 0xa000;
		std::vector<uint8_t> rom_image = target.cartridges.front()->get_segments().front().data;
		_rom_length = (uint16_t)(rom_image.size());

		_rom = new uint8_t[0x2000];
		memcpy(_rom, rom_image.data(), rom_image.size());
		write_to_map(_processorReadMemoryMap, _rom, _rom_address, 0x2000);
	}

	if(_should_automatically_load_media)
	{
		if(target.loadingCommand.length())	// TODO: and automatic loading option enabled
		{
			set_typer_for_string(target.loadingCommand.c_str());
		}

		switch(target.vic20.memory_model)
		{
			case StaticAnalyser::Vic20MemoryModel::Unexpanded:
				set_memory_size(Default);
			break;
			case StaticAnalyser::Vic20MemoryModel::EightKB:
				set_memory_size(ThreeKB);
			break;
			case StaticAnalyser::Vic20MemoryModel::ThirtyTwoKB:
				set_memory_size(ThirtyTwoKB);
			break;
		}
	}
}

void Machine::tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape)
{
	_keyboardVIA->set_control_line_input(KeyboardVIA::Port::A, KeyboardVIA::Line::One, tape->get_input());
}

#pragma mark - Disc

void Machine::install_disk_rom()
{
	if(_driveROM && _c1540)
	{
		_c1540->set_rom(_driveROM.get());
		_c1540->run_for_cycles(2000000);
		_driveROM.reset();
	}
}

#pragma mark - UserPortVIA

uint8_t UserPortVIA::get_port_input(Port port)
{
	if(!port)
	{
		return _portA;	// TODO: bit 6 should be high if there is no tape, low otherwise
	}
	return 0xff;
}

void UserPortVIA::set_control_line_output(Port port, Line line, bool value)
{
//	if(port == Port::A && line == Line::Two) {
//		printf("Tape motor %s\n", value ? "on" : "off");
//	}
}

void UserPortVIA::set_serial_line_state(::Commodore::Serial::Line line, bool value)
{
	switch(line)
	{
		default: break;
		case ::Commodore::Serial::Line::Data: _portA = (_portA & ~0x02) | (value ? 0x02 : 0x00);	break;
		case ::Commodore::Serial::Line::Clock: _portA = (_portA & ~0x01) | (value ? 0x01 : 0x00);	break;
	}
}

void UserPortVIA::set_joystick_state(JoystickInput input, bool value)
{
	if(input != JoystickInput::Right)
	{
		_portA = (_portA & ~input) | (value ? 0 : input);
	}
}

void UserPortVIA::set_port_output(Port port, uint8_t value, uint8_t mask)
{
	// Line 7 of port A is inverted and output as serial ATN
	if(!port)
	{
		std::shared_ptr<::Commodore::Serial::Port> serialPort = _serialPort.lock();
		if(serialPort)
			serialPort->set_output(::Commodore::Serial::Line::Attention, (::Commodore::Serial::LineLevel)!(value&0x80));
	}
}

UserPortVIA::UserPortVIA() : _portA(0xbf) {}

void UserPortVIA::set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort)
{
	_serialPort = serialPort;
}

#pragma mark - KeyboardVIA

KeyboardVIA::KeyboardVIA() : _portB(0xff)
{
	clear_all_keys();
}

void KeyboardVIA::set_key_state(uint16_t key, bool isPressed)
{
	if(isPressed)
		_columns[key & 7] &= ~(key >> 3);
	else
		_columns[key & 7] |= (key >> 3);
}

void KeyboardVIA::clear_all_keys()
{
	memset(_columns, 0xff, sizeof(_columns));
}

uint8_t KeyboardVIA::get_port_input(Port port)
{
	if(!port)
	{
		uint8_t result = 0xff;
		for(int c = 0; c < 8; c++)
		{
			if(!(_activation_mask&(1 << c)))
				result &= _columns[c];
		}
		return result;
	}

	return _portB;
}

void KeyboardVIA::set_port_output(Port port, uint8_t value, uint8_t mask)
{
	if(port)
		_activation_mask = (value & mask) | (~mask);
}

void KeyboardVIA::set_control_line_output(Port port, Line line, bool value)
{
	if(line == Line::Two)
	{
		std::shared_ptr<::Commodore::Serial::Port> serialPort = _serialPort.lock();
		if(serialPort)
		{
			// CB2 is inverted to become serial data; CA2 is inverted to become serial clock
			if(port == Port::A)
				serialPort->set_output(::Commodore::Serial::Line::Clock, (::Commodore::Serial::LineLevel)!value);
			else
				serialPort->set_output(::Commodore::Serial::Line::Data, (::Commodore::Serial::LineLevel)!value);
		}
	}
}

void KeyboardVIA::set_joystick_state(JoystickInput input, bool value)
{
	if(input == JoystickInput::Right)
	{
		_portB = (_portB & ~input) | (value ? 0 : input);
	}
}

void KeyboardVIA::set_serial_port(std::shared_ptr<::Commodore::Serial::Port> serialPort)
{
	_serialPort = serialPort;
}

#pragma mark - SerialPort

void SerialPort::set_input(::Commodore::Serial::Line line, ::Commodore::Serial::LineLevel level)
{
	std::shared_ptr<UserPortVIA> userPortVIA = _userPortVIA.lock();
	if(userPortVIA) userPortVIA->set_serial_line_state(line, (bool)level);
}

void SerialPort::set_user_port_via(std::shared_ptr<UserPortVIA> userPortVIA)
{
	_userPortVIA = userPortVIA;
}
