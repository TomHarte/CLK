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
	_is_running_at_zero_cost(false)
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

		// f7af: find tape header, exit with header in buffer
		// F8C0: Read tape block
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
	if(_typer && operation == CPU6502::BusOperation::ReadOpcode && address == 0xEB1E)
	{
		if(!_typer->type_next_character())
			_typer.reset();
	}
	_tape.run_for_cycles(1);
	if(_c1540) _c1540->run_for_cycles(1);

	if(_use_fast_tape_hack && operation == CPU6502::BusOperation::ReadOpcode)
	{
		if(address == 0xF98E)	_is_running_at_zero_cost = true;
		if(address == 0xff56)	_is_running_at_zero_cost = false;
	}

	return _is_running_at_zero_cost ? 0 : 1;
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

void Machine::set_prg(const char *file_name, size_t length, const uint8_t *data)
{
	// TEST!
	StaticAnalyser::GetTargets(file_name);

	if(length > 2)
	{
		_rom_address = (uint16_t)(data[0] | (data[1] << 8));
		_rom_length = (uint16_t)(length - 2);

		// install in the ROM area if this looks like a ROM; otherwise put on tape and throw into that mechanism
		if(_rom_address == 0xa000)
		{
			_rom = new uint8_t[0x2000];
			memcpy(_rom, &data[2], length - 2);
			write_to_map(_processorReadMemoryMap, _rom, _rom_address, 0x2000);
		}
		else
		{
			set_tape(std::shared_ptr<Storage::Tape::Tape>(new Storage::Tape::PRG(file_name)));
		}
	}
}

#pragma mar - Tape

// LAB_FBDB = new tape byte setup;
// loops at LAB_F92F
// LAB_F8C0 = initiate tape read

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
	if(target.tapes.size())
	{
		_tape.set_tape(target.tapes.front());
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

void Machine::set_tape(std::shared_ptr<Storage::Tape::Tape> tape)
{
//	_tape.set_tape(tape);
//	if(_should_automatically_load_media) set_typer_for_string("LOAD\nRUN\n");
}

void Machine::tape_did_change_input(Tape *tape)
{
	_keyboardVIA->set_control_line_input(KeyboardVIA::Port::A, KeyboardVIA::Line::One, tape->get_input());
}

#pragma mark - Disc

void Machine::set_disk(std::shared_ptr<Storage::Disk::Disk> disk)
{
	// construct the 1540
	_c1540.reset(new ::Commodore::C1540::Machine);

	// attach it to the serial bus
	_c1540->set_serial_bus(_serialBus);

	// hand it the disk
	_c1540->set_disk(disk);

	// install the ROM if it was previously set
	install_disk_rom();

	if(_should_automatically_load_media) set_typer_for_string("LOAD\"*\",8,1\nRUN\n");
}

void Machine::install_disk_rom()
{
	if(_driveROM && _c1540)
	{
		_c1540->set_rom(_driveROM.get());
		_c1540->run_for_cycles(2000000);
		_driveROM.reset();
	}
}

#pragma mark - Typer

int Machine::get_typer_delay()
{
	return get_is_resetting() ? 1*263*60*65 : 0;	// wait a second if resetting
}

int Machine::get_typer_frequency()
{
	return 2*263*65;	// accept a new character every two fields
}

bool Machine::typer_set_next_character(::Utility::Typer *typer, char character, int phase)
{
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

void Tape::process_input_pulse(Storage::Tape::PRG::Pulse pulse)
{
	bool new_input_level = pulse.type == Storage::Tape::PRG::Pulse::Low;
	if(_input_level != new_input_level)
	{
		_input_level = new_input_level;
		if(_delegate) _delegate->tape_did_change_input(this);
	}
}

