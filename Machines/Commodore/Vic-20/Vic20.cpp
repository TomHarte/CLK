//
//  Vic20.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Vic20.hpp"

#include <algorithm>

using namespace Commodore::Vic20;

Machine::Machine() :
	_rom(nullptr)
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
	_userPortVIA->set_delegate(this);
	_keyboardVIA->set_delegate(this);
	_tape.set_delegate(this);

	// establish the memory maps
	memset(_videoMemoryMap, 0, sizeof(_videoMemoryMap));
	memset(_processorReadMemoryMap, 0, sizeof(_processorReadMemoryMap));
	memset(_processorWriteMemoryMap, 0, sizeof(_processorWriteMemoryMap));

	write_to_map(_videoMemoryMap, _characterROM, 0x0000, sizeof(_characterROM));
	write_to_map(_videoMemoryMap, _userBASICMemory, 0x2000, sizeof(_userBASICMemory));
	write_to_map(_videoMemoryMap, _screenMemory, 0x3000, sizeof(_screenMemory));

	write_to_map(_processorReadMemoryMap, _userBASICMemory, 0x0000, sizeof(_userBASICMemory));
	write_to_map(_processorReadMemoryMap, _screenMemory, 0x1000, sizeof(_screenMemory));
	write_to_map(_processorReadMemoryMap, _colorMemory, 0x9400, sizeof(_colorMemory));
	write_to_map(_processorReadMemoryMap, _characterROM, 0x8000, sizeof(_characterROM));
	write_to_map(_processorReadMemoryMap, _basicROM, 0xc000, sizeof(_basicROM));
	write_to_map(_processorReadMemoryMap, _kernelROM, 0xe000, sizeof(_kernelROM));

	write_to_map(_processorWriteMemoryMap, _userBASICMemory, 0x0000, sizeof(_userBASICMemory));
	write_to_map(_processorWriteMemoryMap, _screenMemory, 0x1000, sizeof(_screenMemory));
	write_to_map(_processorWriteMemoryMap, _colorMemory, 0x9400, sizeof(_colorMemory));

//	_debugPort.reset(new ::Commodore::Serial::DebugPort);
//	_debugPort->set_serial_bus(_serialBus);
//	_serialBus->add_port(_debugPort);
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
//	if(operation == CPU6502::BusOperation::ReadOpcode && address == 0xee17) logCount = 500;
//	if(operation == CPU6502::BusOperation::ReadOpcode && logCount) {
//		logCount--;
//		printf("%04x\n", address);
//	}

	// run the phase-1 part of this cycle, in which the VIC accesses memory
	uint16_t video_address = _mos6560->get_address();
	uint8_t video_value = _videoMemoryMap[video_address >> 10] ? _videoMemoryMap[video_address >> 10][video_address & 0x3ff] : 0xff; // TODO
	_mos6560->set_graphics_value(video_value, _colorMemory[video_address & 0x03ff]);

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

		// test for PC at F92F
		if(_use_fast_tape_hack && _tape.has_tape() && address == 0xf92f && operation == CPU6502::BusOperation::ReadOpcode)
		{
			// advance time on the tape and the VIAs until an interrupt is signalled
			while(!_userPortVIA->get_interrupt_line() && !_keyboardVIA->get_interrupt_line())
			{
				_userPortVIA->run_for_half_cycles(2);
				_keyboardVIA->run_for_half_cycles(2);
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

	_userPortVIA->run_for_half_cycles(2);
	_keyboardVIA->run_for_half_cycles(2);
	if(_typer) _typer->update(1);
	_tape.run_for_cycles(1);
	if(_c1540) _c1540->run_for_cycles(1);
	return 1;
}

#pragma mark - 6522 delegate

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
	set_nmi_line(_userPortVIA->get_interrupt_line());
	set_irq_line(_keyboardVIA->get_interrupt_line());
}

#pragma mark - Setup

void Machine::setup_output(float aspect_ratio)
{
	_mos6560.reset(new MOS::MOS6560());
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
			if(_c1540)
			{
				_c1540->set_rom(data);
				_c1540->run_for_cycles(2000000);	// pretend it booted a couple of seconds ago
			}
		return;
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
		write_to_map(_processorReadMemoryMap, _rom, _rom_address, _rom_length);
	}
}

#pragma mar - Tape

void Machine::set_tape(std::shared_ptr<Storage::Tape> tape)
{
	_tape.set_tape(tape);
	set_typer_for_string("LOAD\n");
}

void Machine::tape_did_change_input(Tape *tape)
{
	_keyboardVIA->set_control_line_input(KeyboardVIA::Port::A, KeyboardVIA::Line::One, tape->get_input());
}

#pragma mark - Disc

void Machine::set_disk(std::shared_ptr<Storage::Disk> disk)
{
	// construct the 1540
	_c1540.reset(new ::Commodore::C1540::Machine);

	// attach it to the serial bus
	_c1540->set_serial_bus(_serialBus);

	// hand it the disk
	_c1540->set_disk(disk);
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

