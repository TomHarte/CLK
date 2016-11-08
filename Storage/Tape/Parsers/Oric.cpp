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
	_detection_mode = use_fast_encoding ? FastZero : SlowZero;
	_cycle_length = 0.0f;

	int result = 0;
	int bit_count = 0;
	while(bit_count < 11 && !tape->is_at_end())
	{
		SymbolType symbol = get_next_symbol(tape);
		if(!bit_count && symbol != SymbolType::Zero) continue;
		_detection_mode = use_fast_encoding ? FastData : SlowData;
		result |= ((symbol == SymbolType::One) ? 1 : 0) << bit_count;
		bit_count++;
	}
	// TODO: check parity?
	return tape->is_at_end() ? -1 : ((result >> 1)&0xff);
}

bool Parser::sync_and_get_encoding_speed(const std::shared_ptr<Storage::Tape::Tape> &tape)
{
	_detection_mode = Sync;
	while(!tape->is_at_end())
	{
		SymbolType symbol = get_next_symbol(tape);
		switch(symbol)
		{
			case SymbolType::FoundSlow: return false;
			case SymbolType::FoundFast: return true;
			default: break;
		}
	}
	return false;
}

void Parser::process_pulse(Storage::Tape::Tape::Pulse pulse)
{
	const float length_threshold = 0.0003125f;

	bool wave_is_high = pulse.type == Storage::Tape::Tape::Pulse::High;
	if(wave_is_high != _wave_was_high && _cycle_length > 0.0f)
	{
		if(_cycle_length > 2.0 * length_threshold)
			push_wave(WaveType::Unrecognised);
		else push_wave(_cycle_length < length_threshold ? WaveType::Short : WaveType::Long);

		_cycle_length = 0.0f;
	}
	_wave_was_high = wave_is_high;
	_cycle_length += pulse.length.get_float();
}

void Parser::inspect_waves(const std::vector<WaveType> &waves)
{
	switch(_detection_mode)
	{
		case FastZero:
			if(waves.size() < 2) return;
			if(waves[0] == WaveType::Short && waves[1] == WaveType::Long)
			{
				push_symbol(SymbolType::Zero, 2);
				return;
			}
		break;

		case FastData:
			if(waves.size() < 2) return;
			if(waves[0] == WaveType::Short && waves[1] != WaveType::Unrecognised)
			{
				push_symbol((waves[1] == WaveType::Long) ? SymbolType::Zero : SymbolType::One, 2);
				return;
			}
		break;

		case SlowZero:
			if(waves.size() < 8) return;
			if(
				waves[0] == WaveType::Long && waves[1] == WaveType::Long && waves[2] == WaveType::Long && waves[3] == WaveType::Long &&
				waves[4] == WaveType::Long && waves[5] == WaveType::Long && waves[6] == WaveType::Long && waves[7] == WaveType::Long
			)
			{
				push_symbol(SymbolType::Zero, 8);
				return;
			}
		break;

		case SlowData:
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
			if(waves.size() < 16) return;	// TODO, maybe: if there are any inconsistencies in the first 8, don't return
		break;

		case Sync:
		{
			// Sync is 0x16, either encoded fast or slow; i.e. 0 0110 1000 1
			// So, fast: [short, long]*2, [short, short]*2, [short, long], [short, short], [short, long]*3, [short, short] = 20
			// [short, short] = 1; [short, long] = 0
			// Slow: long*16, short*32, long*8, short*16, long*24, short*16 = 112
			Pattern slow_sync[] =
			{
				{.type = WaveType::Long,	16},
				{.type = WaveType::Short,	32},
				{.type = WaveType::Long,	8},
				{.type = WaveType::Short,	16},
				{.type = WaveType::Long,	24},
				{.type = WaveType::Short,	16},
				{.type = WaveType::Unrecognised}
			};
			Pattern fast_sync[] =
			{
				{.type = WaveType::Short, 1},
				{.type = WaveType::Long, 1},
				{.type = WaveType::Short, 1},
				{.type = WaveType::Long, 1},
				{.type = WaveType::Short, 5},
				{.type = WaveType::Long, 1},
				{.type = WaveType::Short, 3},
				{.type = WaveType::Long, 1},
				{.type = WaveType::Short, 1},
				{.type = WaveType::Long, 1},
				{.type = WaveType::Short, 1},
				{.type = WaveType::Long, 1},
				{.type = WaveType::Short, 2},
				{.type = WaveType::Unrecognised}
			};

			size_t slow_sync_matching_depth = pattern_matching_depth(waves, slow_sync);
			size_t fast_sync_matching_depth = pattern_matching_depth(waves, fast_sync);

			if(slow_sync_matching_depth == 112)
			{
				push_symbol(SymbolType::FoundSlow, 112);
				return;
			}
			if(fast_sync_matching_depth == 20)
			{
				push_symbol(SymbolType::FoundFast, 20);
				return;
			}
			if(slow_sync_matching_depth < waves.size() && fast_sync_matching_depth < waves.size())
			{
				int least_depth = (int)std::min(slow_sync_matching_depth, fast_sync_matching_depth);
				remove_waves(least_depth ? least_depth : 1);
			}

			return;
		}
		break;
	}

	remove_waves(1);
}

size_t Parser::pattern_matching_depth(const std::vector<WaveType> &waves, Pattern *pattern)
{
	size_t depth = 0;
	int pattern_depth = 0;
	while(depth < waves.size() && pattern->type != WaveType::Unrecognised)
	{
		if(waves[depth] != pattern->type) break;
		depth++;
		pattern_depth++;
		if(pattern_depth == pattern->count)
		{
			pattern_depth = 0;
			pattern++;
		}
	}

	return depth;
}
