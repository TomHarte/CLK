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
	_userPortVIA(new UserPortVIA()),
	_keyboardVIA(new KeyboardVIA())
{
	set_reset_line(true);
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	set_reset_line(false);

	// run the phase-1 part of this cycle, in which the VIC accesses memory
	uint16_t video_address = _mos6560->get_address();
	_mos6560->set_graphics_value(read_memory(video_address), _colorMemory[video_address & 0x03ff]);

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
			*value = _userPortVIA->get_register(address - 0x9110);
		}
		else if((address&0xfff0) == 0x9120)
		{
			*value = _keyboardVIA->get_register(address - 0x9120);
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
			_userPortVIA->set_register(address - 0x9110, *value);
		}
		else if((address&0xfff0) == 0x9120)
		{
			_keyboardVIA->set_register(address - 0x9120, *value);
		}
	}

	_userPortVIA->run_for_cycles(1);
	_keyboardVIA->run_for_cycles(1);
	return 1;
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
}
