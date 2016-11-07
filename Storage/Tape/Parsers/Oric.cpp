//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

using namespace Storage::Tape::Oric;

int Parser::get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape, bool use_fast_encoding)
{
	_use_fast_encoding = use_fast_encoding;
	_cycle_length = 0.0f;

	int result = 0;
	int bit_count = 0;
	while(bit_count < 10 && !tape->is_at_end())
	{
		SymbolType symbol = get_next_symbol(tape);
		if(!bit_count && symbol != SymbolType::Zero) continue;
		result |= ((symbol == SymbolType::One) ? 1 : 0) << bit_count;
		bit_count++;
	}
	// TODO: check parity
	return tape->is_at_end() ? -1 : (result >> 1);
}

void Parser::process_pulse(Storage::Tape::Tape::Pulse pulse)
{
	const float length_threshold = 0.0003125f;

	bool wave_is_high = pulse.type == Storage::Tape::Tape::Pulse::High;
	bool did_change = (wave_is_high != _wave_was_high && _cycle_length > 0.0f);
	_cycle_length += pulse.length.get_float();
	if(did_change)
	{
		if(_cycle_length > 2.0 * length_threshold) push_wave(WaveType::Unrecognised);
		else push_wave(_cycle_length < length_threshold ? WaveType::Short : WaveType::Long);

		_cycle_length = 0.0f;
		_wave_was_high = wave_is_high;
	}
}

void Parser::inspect_waves(const std::vector<WaveType> &waves)
{
	if(_use_fast_encoding)
	{
		if(waves.size() < 2) return;
		if(waves[0] == WaveType::Long && waves[1] != WaveType::Unrecognised)
		{
			push_symbol((waves[1] == WaveType::Long) ? SymbolType::Zero : SymbolType::One, 2);
			return;
		}
	}
	else
	{
		if(waves.size() < 16) return;
#define CHECK_RUN(length, type, symbol)	\
		if(waves.size() >= length)\
		{\
			size_t c;\
			for(c = 0; c < length; c++) if(waves[c] != type) break;\
			if(c == length)\
			{\
				push_symbol(symbol, 8);\
				return;\
			}\
		}

		CHECK_RUN(8, WaveType::Long, SymbolType::Zero);
		CHECK_RUN(16, WaveType::Short, SymbolType::One);
#undef CHECK_RUN
	}

	remove_waves(1);
}
