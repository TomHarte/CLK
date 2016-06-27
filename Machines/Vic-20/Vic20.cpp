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
	_tape.set_delegate(this);
	set_reset_line(true);

	printf("User port: %p\n", &_userPortVIA);
	printf("Keyboard: %p\n", &_keyboardVIA);
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
		uint8_t result = read_memory(address);
		if((address&0xff00) == 0x9000)
		{
			result &= _mos6560->get_register(address);
		}
		if((address&0xfc10) == 0x9010)
		{
			result &= _userPortVIA.get_register(address);
		}
		if((address&0xfc20) == 0x9020)
		{
			result &= _keyboardVIA.get_register(address);
		}
		*value = result;
	}
	else
	{
		uint8_t *ram = ram_pointer(address);
		if(ram) *ram = *value;
		if((address&0xff00) == 0x9000)
		{
			_mos6560->set_register(address, *value);
		}
		if((address&0xfc10) == 0x9010)
		{
			_userPortVIA.set_register(address, *value);
		}
		if((address&0xfc20) == 0x9020)
		{
			_keyboardVIA.set_register(address, *value);
		}
	}

	_userPortVIA.run_for_half_cycles(2);
	_keyboardVIA.run_for_half_cycles(2);
	if(_typer) _typer->update(1);
	_tape.run_for_cycles(1);
	return 1;
}

#pragma mark - 6522 delegate

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
//	bool irq = _userPortVIA.get_interrupt_line() || _keyboardVIA.get_interrupt_line();
	set_nmi_line(_userPortVIA.get_interrupt_line());
	set_irq_line(_keyboardVIA.get_interrupt_line());
}

#pragma mark - Setup

void Machine::setup_output(float aspect_ratio)
{
	_mos6560.reset(new MOS::MOS6560());
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

#pragma mar - Tape

void Machine::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape.set_tape(tape);
}

void Machine::tape_did_change_input(Tape *tape)
{
	_keyboardVIA.set_control_line(KeyboardVIA::Port::A, KeyboardVIA::Line::One, tape->get_input());
}

#pragma mark - Typer

int Machine::get_typer_delay()
{
	return get_reset_line() ? 1*263*60*65 : 0;	// wait two seconds if resetting
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

		{KeyLShift, Key1, TerminateSequence},	{KeyLShift, Key2, TerminateSequence},		// !, "
		{KeyLShift, Key3, TerminateSequence},	{KeyLShift, Key4, TerminateSequence},		// #, $
		{KeyLShift, Key5, TerminateSequence},	{KeyLShift, Key6, TerminateSequence},		// %, &
		{KeyLShift, Key7, TerminateSequence},	{KeyLShift, Key8, TerminateSequence},		// ', (
		{KeyLShift, Key9, TerminateSequence},	{KeyAsterisk, TerminateSequence},			// ), *
		{KeyPlus, TerminateSequence},			{KeyComma, TerminateSequence},				// +, ,
		{KeyDash, TerminateSequence},			{KeyFullStop, TerminateSequence},			// -, .
		{KeySlash, TerminateSequence},			// /

		{Key0, TerminateSequence},				{Key1, TerminateSequence},					// 0, 1
		{Key2, TerminateSequence},				{Key3, TerminateSequence},					// 2, 3
		{Key4, TerminateSequence},				{Key5, TerminateSequence},					// 4, 5
		{Key6, TerminateSequence},				{Key7, TerminateSequence},					// 6, 7
		{Key8, TerminateSequence},				{Key9, TerminateSequence},					// 8, 9

		{KeyColon, TerminateSequence},					{KeySemicolon, TerminateSequence},		// :, ;
		{KeyLShift, KeyComma, TerminateSequence},		{KeyEquals, TerminateSequence},			// <, =
		{KeyLShift, KeyFullStop, TerminateSequence},	{KeyLShift, KeySlash, TerminateSequence},		// >, ?
		{KeyAt, TerminateSequence},						// @

		{KeyA, TerminateSequence},	{KeyB, TerminateSequence},	{KeyC, TerminateSequence},	{KeyD, TerminateSequence},	// A, B, C, D
		{KeyE, TerminateSequence},	{KeyF, TerminateSequence},	{KeyG, TerminateSequence},	{KeyH, TerminateSequence},	// E, F, G, H
		{KeyI, TerminateSequence},	{KeyJ, TerminateSequence},	{KeyK, TerminateSequence},	{KeyL, TerminateSequence},	// I, J, K L
		{KeyM, TerminateSequence},	{KeyN, TerminateSequence},	{KeyO, TerminateSequence},	{KeyP, TerminateSequence},	// M, N, O, P
		{KeyQ, TerminateSequence},	{KeyR, TerminateSequence},	{KeyS, TerminateSequence},	{KeyT, TerminateSequence},	// Q, R, S, T
		{KeyU, TerminateSequence},	{KeyV, TerminateSequence},	{KeyW, TerminateSequence},	{KeyX, TerminateSequence},	// U, V, W X
		{KeyY, TerminateSequence},	{KeyZ, TerminateSequence},	// Y, Z

		{KeyLShift, KeyColon, TerminateSequence},		{NotMapped},	// [, '\'
		{KeyLShift, KeyFullStop, TerminateSequence},	{NotMapped},	// ], ^
		{NotMapped},									{NotMapped},	// _, `

		{KeyA, TerminateSequence},	{KeyB, TerminateSequence},	{KeyC, TerminateSequence},	{KeyD, TerminateSequence},	// A, B, C, D
		{KeyE, TerminateSequence},	{KeyF, TerminateSequence},	{KeyG, TerminateSequence},	{KeyH, TerminateSequence},	// E, F, G, H
		{KeyI, TerminateSequence},	{KeyJ, TerminateSequence},	{KeyK, TerminateSequence},	{KeyL, TerminateSequence},	// I, J, K L
		{KeyM, TerminateSequence},	{KeyN, TerminateSequence},	{KeyO, TerminateSequence},	{KeyP, TerminateSequence},	// M, N, O, P
		{KeyQ, TerminateSequence},	{KeyR, TerminateSequence},	{KeyS, TerminateSequence},	{KeyT, TerminateSequence},	// Q, R, S, T
		{KeyU, TerminateSequence},	{KeyV, TerminateSequence},	{KeyW, TerminateSequence},	{KeyX, TerminateSequence},	// U, V, W X
		{KeyY, TerminateSequence},	{KeyZ, TerminateSequence},	// Y, Z
//		{KeyLShift, KeyA, TerminateSequence},					{KeyLShift, KeyB, TerminateSequence},					// a, b
//		{KeyLShift, KeyC, TerminateSequence},					{KeyLShift, KeyD, TerminateSequence},					// c, d
//		{KeyLShift, KeyE, TerminateSequence},					{KeyLShift, KeyF, TerminateSequence},					// e, f
//		{KeyLShift, KeyG, TerminateSequence},					{KeyLShift, KeyH, TerminateSequence},					// g, h
//		{KeyLShift, KeyI, TerminateSequence},					{KeyLShift, KeyJ, TerminateSequence},					// i, j
//		{KeyLShift, KeyK, TerminateSequence},					{KeyLShift, KeyL, TerminateSequence},					// k, l
//		{KeyLShift, KeyM, TerminateSequence},					{KeyLShift, KeyN, TerminateSequence},					// m, n
//		{KeyLShift, KeyO, TerminateSequence},					{KeyLShift, KeyP, TerminateSequence},					// o, p
//		{KeyLShift, KeyQ, TerminateSequence},					{KeyLShift, KeyR, TerminateSequence},					// q, r
//		{KeyLShift, KeyS, TerminateSequence},					{KeyLShift, KeyT, TerminateSequence},					// s, t
//		{KeyLShift, KeyU, TerminateSequence},					{KeyLShift, KeyV, TerminateSequence},					// u, v
//		{KeyLShift, KeyW, TerminateSequence},					{KeyLShift, KeyX, TerminateSequence},					// w, x
//		{KeyLShift, KeyY, TerminateSequence},					{KeyLShift, KeyZ, TerminateSequence},					// y, z

	};
	Key *key_sequence = nullptr;

	character &= 0x7f;
	if(character < sizeof(key_sequences) / sizeof(*key_sequences))
	{
		key_sequence = key_sequences[character];

		if(key_sequence[0] != NotMapped)
		{
			if(phase > 0)
			{
				set_key_state(key_sequence[phase-1], true);
				return key_sequence[phase] == TerminateSequence;
			}
			else
				return false;
		}
	}

	return true;
}

#pragma mark - Tape

Tape::Tape() : TapePlayer(1022727) {}

void Tape::set_motor_control(bool enabled) {}
void Tape::set_tape_output(bool set) {}

void Tape::process_input_pulse(Storage::Tape::Pulse pulse)
{
	bool new_input_level = pulse.type == Storage::Tape::Pulse::Low;
	if(_input_level != new_input_level)
	{
		_input_level = new_input_level;
		if(_delegate) _delegate->tape_did_change_input(this);
	}
}
