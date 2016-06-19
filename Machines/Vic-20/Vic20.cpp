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
	if(_typer) _typer->update(1);
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
			set_typer_for_string("RUN\n");
		}

		_rom = new uint8_t[length - 2];
		memcpy(_rom, &data[2], length - 2);
	}
}

#pragma mark - Typer

int Machine::get_typer_delay()
{
	return 1*263*60*65;	// wait two seconds
}

int Machine::get_typer_frequency()
{
	return 2*263*65;	// accept a new character every two fields
}

bool Machine::typer_set_next_character(::Utility::Typer *typer, char character, int phase)
{
	// If there's a 'ROM' installed that can never be accessed, assume that this typing was scheduled because
	// it should be in RAM. So copy it there.
	if(_rom && _rom_address >= 0x1000 && _rom_address+_rom_length < 0x2000)
	{
		memcpy(&_screenMemory[_rom_address - 0x1000], _rom, _rom_length);
		delete[] _rom;
		_rom = nullptr;
	}

	if(!phase) clear_all_keys();

	// The following table is arranged in ASCII order
	Key key_sequences[][3] = {
		{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},
		{KeyDelete, TerminateSequence},
		{NotMapped},
		{KeyReturn, TerminateSequence},
		{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},
		{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},
		{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},
		{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},
		{NotMapped},	{NotMapped},	{NotMapped},	{NotMapped},

		{KeySpace, TerminateSequence},				// space

		{KeyLShift, Key1, TerminateSequence},		// !
		{KeyLShift, Key2, TerminateSequence},		// "
		{KeyLShift, Key3, TerminateSequence},		// #
		{KeyLShift, Key4, TerminateSequence},		// $
		{KeyLShift, Key5, TerminateSequence},		// %
		{KeyLShift, Key6, TerminateSequence},		// &
		{KeyLShift, Key7, TerminateSequence},		// '
		{KeyLShift, Key8, TerminateSequence},		// (
		{KeyLShift, Key9, TerminateSequence},		// )

		{KeyAsterisk, TerminateSequence},			// *
		{KeyPlus, TerminateSequence},				// +
		{KeyComma, TerminateSequence},				// ,
		{KeyDash, TerminateSequence},				// -
		{KeyFullStop, TerminateSequence},			// .
		{KeySlash, TerminateSequence},				// /

		{Key0, TerminateSequence},		// 0
		{Key1, TerminateSequence},		// 1
		{Key2, TerminateSequence},		// 2
		{Key3, TerminateSequence},		// 3
		{Key4, TerminateSequence},		// 4
		{Key5, TerminateSequence},		// 5
		{Key6, TerminateSequence},		// 6
		{Key7, TerminateSequence},		// 7
		{Key8, TerminateSequence},		// 8
		{Key9, TerminateSequence},		// 9

		{KeyColon, TerminateSequence},					// :
		{KeySemicolon, TerminateSequence},				// ;
		{KeyLShift, KeyComma, TerminateSequence},		// <
		{KeyEquals, TerminateSequence},					// =
		{KeyLShift, KeyFullStop, TerminateSequence},	// >
		{KeyLShift, KeySlash, TerminateSequence},		// ?
		{KeyAt, TerminateSequence},						// @

		{KeyA, TerminateSequence},					// A
		{KeyB, TerminateSequence},					// B
		{KeyC, TerminateSequence},					// C
		{KeyD, TerminateSequence},					// D
		{KeyE, TerminateSequence},					// E
		{KeyF, TerminateSequence},					// F
		{KeyG, TerminateSequence},					// G
		{KeyH, TerminateSequence},					// H
		{KeyI, TerminateSequence},					// I
		{KeyJ, TerminateSequence},					// J
		{KeyK, TerminateSequence},					// K
		{KeyL, TerminateSequence},					// L
		{KeyM, TerminateSequence},					// M
		{KeyN, TerminateSequence},					// N
		{KeyO, TerminateSequence},					// O
		{KeyP, TerminateSequence},					// P
		{KeyQ, TerminateSequence},					// Q
		{KeyR, TerminateSequence},					// R
		{KeyS, TerminateSequence},					// S
		{KeyT, TerminateSequence},					// T
		{KeyU, TerminateSequence},					// U
		{KeyV, TerminateSequence},					// V
		{KeyW, TerminateSequence},					// W
		{KeyX, TerminateSequence},					// X
		{KeyY, TerminateSequence},					// Y
		{KeyZ, TerminateSequence},					// Z

		{KeyLShift, KeyColon, TerminateSequence},		// [
		{NotMapped},									// '\'
		{KeyLShift, KeyFullStop, TerminateSequence},	// ]
		{NotMapped},									// ^
		{NotMapped},									// _
		{NotMapped},									// `

		{KeyLShift, KeyA, TerminateSequence},					// A
		{KeyLShift, KeyB, TerminateSequence},					// B
		{KeyLShift, KeyC, TerminateSequence},					// C
		{KeyLShift, KeyD, TerminateSequence},					// D
		{KeyLShift, KeyE, TerminateSequence},					// E
		{KeyLShift, KeyF, TerminateSequence},					// F
		{KeyLShift, KeyG, TerminateSequence},					// G
		{KeyLShift, KeyH, TerminateSequence},					// H
		{KeyLShift, KeyI, TerminateSequence},					// I
		{KeyLShift, KeyJ, TerminateSequence},					// J
		{KeyLShift, KeyK, TerminateSequence},					// K
		{KeyLShift, KeyL, TerminateSequence},					// L
		{KeyLShift, KeyM, TerminateSequence},					// M
		{KeyLShift, KeyN, TerminateSequence},					// N
		{KeyLShift, KeyO, TerminateSequence},					// O
		{KeyLShift, KeyP, TerminateSequence},					// P
		{KeyLShift, KeyQ, TerminateSequence},					// Q
		{KeyLShift, KeyR, TerminateSequence},					// R
		{KeyLShift, KeyS, TerminateSequence},					// S
		{KeyLShift, KeyT, TerminateSequence},					// T
		{KeyLShift, KeyU, TerminateSequence},					// U
		{KeyLShift, KeyV, TerminateSequence},					// V
		{KeyLShift, KeyW, TerminateSequence},					// W
		{KeyLShift, KeyX, TerminateSequence},					// X
		{KeyLShift, KeyY, TerminateSequence},					// Y
		{KeyLShift, KeyZ, TerminateSequence},					// Z

	};
	Key *key_sequence = nullptr;

	character &= 0x7f;
	if(character < sizeof(key_sequences) / sizeof(*key_sequences))
	{
		key_sequence = key_sequences[character];
	}

	if(key_sequence && key_sequence[phase] != NotMapped)
	{
		set_key_state(key_sequence[phase], true);
		return key_sequence[phase+1] == TerminateSequence;
	}

	return true;
}
