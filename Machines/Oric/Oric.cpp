//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"
#include "../MemoryFuzzer.hpp"

using namespace Oric;

Machine::Machine() :
	_cycles_since_video_update(0),
	_use_fast_tape_hack(false),
	_typer_delay(2500000)
{
	set_clock_rate(1000000);
	_via.tape.reset(new TapePlayer);
	_via.set_interrupt_delegate(this);
	_keyboard.reset(new Keyboard);
	_via.keyboard = _keyboard;
	clear_all_keys();
	_via.tape->set_delegate(this);
	Memory::Fuzz(_ram, sizeof(_ram));
}

void Machine::configure_as_target(const StaticAnalyser::Target &target)
{
	if(target.tapes.size())
	{
		_via.tape->set_tape(target.tapes.front());
	}

	if(target.loadingCommand.length())	// TODO: and automatic loading option enabled
	{
		set_typer_for_string(target.loadingCommand.c_str());
	}
}

void Machine::set_rom(std::vector<uint8_t> data)
{
	memcpy(_rom, data.data(), std::min(data.size(), sizeof(_rom)));
}

unsigned int Machine::perform_bus_operation(CPU6502::BusOperation operation, uint16_t address, uint8_t *value)
{
	if(address >= 0xc000)
	{
		if(isReadOperation(operation)) *value = _rom[address&16383];

		// 024D = 0 => fast; otherwise slow
		// E6C9 = read byte: return byte in A
		if(address == 0xe6c9 && _use_fast_tape_hack && operation == CPU6502::BusOperation::ReadOpcode)
		{
			uint8_t next_byte = _via.tape->get_next_byte(!_ram[0x024d]);
			set_value_of_register(CPU6502::A, next_byte);
			set_value_of_register(CPU6502::Flags, next_byte ? 0 : CPU6502::Flag::Zero);
			*value = 0x60; // i.e. RTS
		}
	}
	else
	{
		if((address & 0xff00) == 0x0300)
		{
			if(isReadOperation(operation)) *value = _via.get_register(address);
			else _via.set_register(address, *value);
		}
		else
		{
			if(isReadOperation(operation))
				*value = _ram[address];
			else
			{
				if(address >= 0x9800) { update_video(); _typer_delay = 0; }
				_ram[address] = *value;
			}
		}
	}

	if(_typer && operation == CPU6502::BusOperation::ReadOpcode && address == 0xF495)
	{
		if(!_typer->type_next_character()) _typer.reset();
	}

	_via.run_for_cycles(1);
	_cycles_since_video_update++;
	return 1;
}

void Machine::synchronise()
{
	update_video();
	_via.synchronise();
}

void Machine::update_video()
{
	_videoOutput->run_for_cycles(_cycles_since_video_update);
	_cycles_since_video_update = 0;
}

void Machine::setup_output(float aspect_ratio)
{
	_videoOutput.reset(new VideoOutput(_ram));
	_via.ay8910.reset(new GI::AY38910());
	_via.ay8910->set_clock_rate(1000000);
}

void Machine::close_output()
{
	_videoOutput.reset();
	_via.ay8910.reset();
}

void Machine::mos6522_did_change_interrupt_status(void *mos6522)
{
	set_irq_line(_via.get_interrupt_line());
}

void Machine::set_key_state(Key key, bool isPressed)
{
	if(key == KeyNMI)
	{
		set_nmi_line(isPressed);
	}
	else
	{
		if(isPressed)
			_keyboard->rows[key >> 8] |= (key & 0xff);
		else
			_keyboard->rows[key >> 8] &= ~(key & 0xff);
	}
}

void Machine::clear_all_keys()
{
	memset(_keyboard->rows, 0, sizeof(_keyboard->rows));
}

void Machine::set_use_fast_tape_hack(bool activate)
{
	_use_fast_tape_hack = activate;
}

void Machine::tape_did_change_input(Storage::Tape::BinaryTapePlayer *tape_player)
{
	// set CB1
	_via.set_control_line_input(VIA::Port::B, VIA::Line::One, tape_player->get_input());
}

std::shared_ptr<Outputs::CRT::CRT> Machine::get_crt()
{
	return _videoOutput->get_crt();
}

std::shared_ptr<Outputs::Speaker> Machine::get_speaker()
{
	return _via.ay8910;
}

void Machine::run_for_cycles(int number_of_cycles)
{
	CPU6502::Processor<Machine>::run_for_cycles(number_of_cycles);
}

#pragma mark - Automatic typing

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

		{KeyLeftShift, Key1, TerminateSequence},		{KeyLeftShift, KeyQuote, TerminateSequence},	// !, "
		{KeyLeftShift, Key3, TerminateSequence},		{KeyLeftShift, Key4, TerminateSequence},		// #, $
		{KeyLeftShift, Key5, TerminateSequence},		{KeyLeftShift, Key7, TerminateSequence},		// %, &
		{KeyQuote, TerminateSequence},					{KeyLeftShift, Key9, TerminateSequence},		// ', (
		{KeyLeftShift, Key0, TerminateSequence},		{KeyLeftShift, Key8, TerminateSequence},		// ), *
		{KeyLeftShift, KeyEquals, TerminateSequence},	{KeyComma, TerminateSequence},				// +, ,
		{KeyMinus, TerminateSequence},					{KeyFullStop, TerminateSequence},			// -, .
		{KeyForwardSlash, TerminateSequence},			// /

		{Key0, TerminateSequence},				{Key1, TerminateSequence},					// 0, 1
		{Key2, TerminateSequence},				{Key3, TerminateSequence},					// 2, 3
		{Key4, TerminateSequence},				{Key5, TerminateSequence},					// 4, 5
		{Key6, TerminateSequence},				{Key7, TerminateSequence},					// 6, 7
		{Key8, TerminateSequence},				{Key9, TerminateSequence},					// 8, 9

		{KeyLeftShift, KeySemiColon, TerminateSequence},	{KeySemiColon, TerminateSequence},		// :, ;
		{KeyLeftShift, KeyComma, TerminateSequence},		{KeyEquals, TerminateSequence},			// <, =
		{KeyLeftShift, KeyFullStop, TerminateSequence},		{KeyLeftShift, KeyForwardSlash, TerminateSequence},		// >, ?
		{KeyLeftShift, Key2, TerminateSequence},						// @

		{KeyLeftShift, KeyA, TerminateSequence},	{KeyLeftShift, KeyB, TerminateSequence},	{KeyLeftShift, KeyC, TerminateSequence},	{KeyLeftShift, KeyD, TerminateSequence},	// A, B, C, D
		{KeyLeftShift, KeyE, TerminateSequence},	{KeyLeftShift, KeyF, TerminateSequence},	{KeyLeftShift, KeyG, TerminateSequence},	{KeyLeftShift, KeyH, TerminateSequence},	// E, F, G, H
		{KeyLeftShift, KeyI, TerminateSequence},	{KeyLeftShift, KeyJ, TerminateSequence},	{KeyLeftShift, KeyK, TerminateSequence},	{KeyLeftShift, KeyL, TerminateSequence},	// I, J, K L
		{KeyLeftShift, KeyM, TerminateSequence},	{KeyLeftShift, KeyN, TerminateSequence},	{KeyLeftShift, KeyO, TerminateSequence},	{KeyLeftShift, KeyP, TerminateSequence},	// M, N, O, P
		{KeyLeftShift, KeyQ, TerminateSequence},	{KeyLeftShift, KeyR, TerminateSequence},	{KeyLeftShift, KeyS, TerminateSequence},	{KeyLeftShift, KeyT, TerminateSequence},	// Q, R, S, T
		{KeyLeftShift, KeyU, TerminateSequence},	{KeyLeftShift, KeyV, TerminateSequence},	{KeyLeftShift, KeyW, TerminateSequence},	{KeyLeftShift, KeyX, TerminateSequence},	// U, V, W X
		{KeyLeftShift, KeyY, TerminateSequence},	{KeyLeftShift, KeyZ, TerminateSequence},	// Y, Z

		{KeyOpenSquare, TerminateSequence},		{KeyBackSlash, TerminateSequence},			// [, '\'
		{KeyCloseSquare, TerminateSequence},	{KeyLeftShift, Key6, TerminateSequence},	// ], ^
		{NotMapped},							{NotMapped},	// _, `

		{KeyA, TerminateSequence},	{KeyB, TerminateSequence},	{KeyC, TerminateSequence},	{KeyD, TerminateSequence},	// A, B, C, D
		{KeyE, TerminateSequence},	{KeyF, TerminateSequence},	{KeyG, TerminateSequence},	{KeyH, TerminateSequence},	// E, F, G, H
		{KeyI, TerminateSequence},	{KeyJ, TerminateSequence},	{KeyK, TerminateSequence},	{KeyL, TerminateSequence},	// I, J, K L
		{KeyM, TerminateSequence},	{KeyN, TerminateSequence},	{KeyO, TerminateSequence},	{KeyP, TerminateSequence},	// M, N, O, P
		{KeyQ, TerminateSequence},	{KeyR, TerminateSequence},	{KeyS, TerminateSequence},	{KeyT, TerminateSequence},	// Q, R, S, T
		{KeyU, TerminateSequence},	{KeyV, TerminateSequence},	{KeyW, TerminateSequence},	{KeyX, TerminateSequence},	// U, V, W X
		{KeyY, TerminateSequence},	{KeyZ, TerminateSequence},	// Y, Z

		{KeyLeftShift, KeyOpenSquare, TerminateSequence},		{KeyLeftShift, KeyBackSlash, TerminateSequence},			// {, |
		{KeyLeftShift, KeyCloseSquare, TerminateSequence},	// }, ~
	};
	Key *key_sequence = nullptr;

	character &= 0x7f;
	if(character < sizeof(key_sequences) / sizeof(*key_sequences))
	{
		key_sequence = key_sequences[character];

		if(key_sequence[0] != NotMapped)
		{
			if(phase)
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

#pragma mark - The 6522

Machine::VIA::VIA() : MOS::MOS6522<Machine::VIA>(), _cycles_since_ay_update(0) {}

void Machine::VIA::set_control_line_output(Port port, Line line, bool value)
{
	if(line)
	{
		if(port) _ay_bdir = value; else _ay_bc1 = value;
		update_ay();
	}
}

void Machine::VIA::set_port_output(Port port, uint8_t value, uint8_t direction_mask)
{
	if(port)
	{
		keyboard->row = value;
		tape->set_motor_control(value & 0x40);
	}
	else
	{
		ay8910->set_data_input(value);
	}
}

uint8_t Machine::VIA::get_port_input(Port port)
{
	if(port)
	{
		uint8_t column = ay8910->get_port_output(false) ^ 0xff;
		return (keyboard->rows[keyboard->row & 7] & column) ? 0x08 : 0x00;
	}
	else
	{
		return ay8910->get_data_output();
	}
}

void Machine::VIA::synchronise()
{
	ay8910->run_for_cycles(_cycles_since_ay_update);
	_cycles_since_ay_update = 0;
}

void Machine::VIA::run_for_cycles(unsigned int number_of_cycles)
{
	_cycles_since_ay_update += number_of_cycles;
	MOS::MOS6522<VIA>::run_for_cycles(number_of_cycles);
	tape->run_for_cycles((int)number_of_cycles);
}

void Machine::VIA::update_ay()
{
	ay8910->run_for_cycles(_cycles_since_ay_update);
	_cycles_since_ay_update = 0;
	ay8910->set_control_lines( (GI::AY38910::ControlLines)((_ay_bdir ? GI::AY38910::BCDIR : 0) | (_ay_bc1 ? GI::AY38910::BC1 : 0) | GI::AY38910::BC2));
}

#pragma mark - TapePlayer

Machine::TapePlayer::TapePlayer() :
	Storage::Tape::BinaryTapePlayer(1000000),
	_is_catching_bytes(false)
{}

uint8_t Machine::TapePlayer::get_next_byte(bool fast)
{
	_is_in_fast_mode = fast;
	_is_catching_bytes = true;

	_was_high = get_input();
	_queued_lengths_pointer = 0;
	_data_register = 0;
	_cycle_length = 0.0f;

	_bit_count = 0;
	while(_bit_count < 10)
	{
		process_next_event();
	}

	_is_catching_bytes = false;
	return (uint8_t)(_data_register >> 1);
}

void Machine::TapePlayer::process_input_pulse(Storage::Tape::Tape::Pulse pulse)
{
	Storage::Tape::BinaryTapePlayer::process_input_pulse(pulse);

	if(_is_catching_bytes)
	{
		_cycle_length += pulse.length.get_float();
		bool is_high = get_input();
		if(is_high != _was_high)
		{
			// queue up the new length
			_queued_lengths[_queued_lengths_pointer] = _cycle_length;
			_cycle_length = 0.0f;
			_queued_lengths_pointer++;

			// search for bits
			if(_is_in_fast_mode)
			{
				if(_queued_lengths_pointer >= 2)
				{
					float first_two = _queued_lengths[0] + _queued_lengths[1];
					if(first_two < 0.000512*2.0 && _queued_lengths[0] >= _queued_lengths[1] - 0.000256)
					{
						int new_bit = (first_two < 0.000512) ? 1 : 0;
						if(_bit_count || !new_bit)
						{
							_data_register |= (new_bit << _bit_count);
							_bit_count++;
						}
						memmove(_queued_lengths, &_queued_lengths[2], sizeof(float)*14);
						_queued_lengths_pointer -= 2;
					}
					else
					{
						memmove(_queued_lengths, &_queued_lengths[1], sizeof(float)*15);
						_queued_lengths_pointer--;
					}
				}
			}
			else
			{
				// TODO
			}
		}
		_was_high = is_high;
	}
}

void Machine::TapePlayer::run_for_cycles(int number_of_cycles)
{
	if(!_is_catching_bytes) Storage::Tape::BinaryTapePlayer::run_for_cycles(number_of_cycles);
}
