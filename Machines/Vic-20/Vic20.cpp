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

Machine::Machine() :
	_rom(nullptr)
{
	_userPortVIA.set_delegate(this);
	_keyboardVIA.set_delegate(this);
	set_reset_line(true);
}

Machine::~Machine()
{
	delete[] _rom;
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	set_reset_line(false);

	// run the phase-1 part of this cycle, in which the VIC accesses memory
	uint16_t video_address = _mos6560->get_address();
	uint8_t video_value = 0xff; // TODO
	if(!(video_address&0x2000))
	{
		video_value = _characterROM[video_address & 0x0fff];
	}
	else
	{
		video_address &= 0x1fff;
		if(video_address < sizeof(_userBASICMemory)) video_value = _userBASICMemory[video_address];
		else if(video_address >= 0x1000 && video_address < 0x2000) video_value = _screenMemory[video_address&0x0fff];
	}
	_mos6560->set_graphics_value(video_value, _colorMemory[video_address & 0x03ff]);

	// run the phase-2 part of the cycle, which is whatever the 6502 said it should be
	if(isReadOperation(operation))
	{
		*value = read_memory(address);
		if((address&0xfff0) == 0x9000)
		{
			*value = _mos6560->get_register(address - 0x9000);
		}
		else if((address&0xfff0) == 0x9110)
		{
			*value = _userPortVIA.get_register(address - 0x9110);
		}
		else if((address&0xfff0) == 0x9120)
		{
			*value = _keyboardVIA.get_register(address - 0x9120);
		}
	}
	else
	{
		uint8_t *ram = ram_pointer(address);
		if(ram) *ram = *value;
		else if((address&0xfff0) == 0x9000)
		{
			_mos6560->set_register(address - 0x9000, *value);
		}
		else if((address&0xfff0) == 0x9110)
		{
			_userPortVIA.set_register(address - 0x9110, *value);
		}
		else if((address&0xfff0) == 0x9120)
		{
			_keyboardVIA.set_register(address - 0x9120, *value);
		}
	}

	_userPortVIA.run_for_half_cycles(2);
	_keyboardVIA.run_for_half_cycles(2);
	return 1;
}

#pragma mark - 6522 delegate

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
	bool irq = _userPortVIA.get_interrupt_line() || _keyboardVIA.get_interrupt_line();
	set_irq_line(irq);
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
	if(length > 2)
	{
		_rom_address = (uint16_t)(data[0] | (data[1] << 8));
		_rom_length = (uint16_t)(length - 2);
		if(_rom_address >= 0x1000 && _rom_address+_rom_length < 0x2000)
		{
			memcpy(&_screenMemory[_rom_address - 0x1000], &data[2], length - 2);
		}
		else
		{
			_rom = new uint8_t[length - 2];
			memcpy(_rom, &data[2], length - 2);
		}
	}
}
