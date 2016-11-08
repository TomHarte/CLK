//
//  Acorn.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Acorn.hpp"

using namespace Storage::Tape::Acorn;

Parser::Parser() :
	::Storage::Tape::Parser<WaveType, SymbolType>(),
	_crc(0x1021, 0x0000) {}

int Parser::get_next_bit(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	SymbolType symbol = get_next_symbol(tape);
	return (symbol == SymbolType::One) ? 1 : 0;
}

int Parser::get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	int value = 0;
	int c = 8;
	if(get_next_bit(tape))
	{
		set_error_flag();
		return -1;
	}
	while(c--)
	{
		value = (value >> 1) | (get_next_bit(tape) << 7);
	}
	if(!get_next_bit(tape))
	{
		set_error_flag();
		return -1;
	}
	_crc.add((uint8_t)value);
	return value;
}

int Parser::get_next_short(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	int result = get_next_byte(tape);
	result |= get_next_byte(tape) << 8;
	return result;
}

int Parser::get_next_word(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	int result = get_next_short(tape);
	result |= get_next_short(tape) << 8;
	return result;
}

void Parser::reset_crc()	{	_crc.reset();				}
uint16_t Parser::get_crc()	{	return _crc.get_value();	}

void Parser::process_pulse(Storage::Tape::Tape::Pulse pulse)
{
	switch(pulse.type)
	{
		default: break;
		case Storage::Tape::Tape::Pulse::High:
		case Storage::Tape::Tape::Pulse::Low:
			float pulse_length = pulse.length.get_float();
			if(pulse_length >= 0.35 / 2400.0 && pulse_length < 0.7 / 2400.0) { push_wave(WaveType::Short); return; }
			if(pulse_length >= 0.35 / 1200.0 && pulse_length < 0.7 / 1200.0) { push_wave(WaveType::Long); return; }
		break;
	}

	push_wave(WaveType::Unrecognised);
}

void Parser::inspect_waves(const std::vector<WaveType> &waves)
{
	if(waves.size() < 2) return;

	if(waves[0] == WaveType::Long && waves[1] == WaveType::Long)
	{
		push_symbol(SymbolType::Zero, 2);
		return;
	}

	if(waves.size() < 4) return;

	if(	waves[0] == WaveType::Short &&
		waves[1] == WaveType::Short &&
		waves[2] == WaveType::Short &&
		waves[3] == WaveType::Short)
	{
		push_symbol(SymbolType::One, 4);
		return;
	}

	remove_waves(1);
}
