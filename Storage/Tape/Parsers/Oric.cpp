//
//  Oric.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Oric.hpp"

using namespace Storage::Tape::Oric;

int Parser::get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape, bool use_fast_encoding) {
	detection_mode_ = use_fast_encoding ? FastZero : SlowZero;
	cycle_length_ = 0.0f;

	int result = 0;
	int bit_count = 0;
	while(bit_count < 11 && !tape->is_at_end()) {
		SymbolType symbol = get_next_symbol(tape);
		if(!bit_count && symbol != SymbolType::Zero) continue;
		detection_mode_ = use_fast_encoding ? FastData : SlowData;
		result |= ((symbol == SymbolType::One) ? 1 : 0) << bit_count;
		bit_count++;
	}
	// TODO: check parity?
	return tape->is_at_end() ? -1 : ((result >> 1)&0xff);
}

bool Parser::sync_and_get_encoding_speed(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	detection_mode_ = Sync;
	while(!tape->is_at_end()) {
		const SymbolType symbol = get_next_symbol(tape);
		switch(symbol) {
			case SymbolType::FoundSlow: return false;
			case SymbolType::FoundFast: return true;
			default: break;
		}
	}
	return false;
}

void Parser::process_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	constexpr float maximum_short_length = 0.000512f;
	constexpr float maximum_medium_length = 0.000728f;
	constexpr float maximum_long_length = 0.001456f;

	bool wave_is_high = pulse.type == Storage::Tape::Tape::Pulse::High;
	if(!wave_was_high_ && wave_is_high != wave_was_high_) {
		if(cycle_length_ < maximum_short_length) push_wave(WaveType::Short);
		else if(cycle_length_ < maximum_medium_length) push_wave(WaveType::Medium);
		else if(cycle_length_ < maximum_long_length) push_wave(WaveType::Long);
		else push_wave(WaveType::Unrecognised);

		cycle_length_ = 0.0f;
	}
	wave_was_high_ = wave_is_high;
	cycle_length_ += pulse.length.get<float>();
}

void Parser::inspect_waves(const std::vector<WaveType> &waves) {
	switch(detection_mode_) {
		case FastZero:
			if(waves.empty()) return;
			if(waves[0] == WaveType::Medium) {
				push_symbol(SymbolType::Zero, 1);
				return;
			}
		break;

		case FastData:
			if(waves.empty()) return;
			if(waves[0] == WaveType::Medium) {
				push_symbol(SymbolType::Zero, 1);
				return;
			}
			if(waves[0] == WaveType::Short) {
				push_symbol(SymbolType::One, 1);
				return;
			}
		break;

		case SlowZero:
			if(waves.size() < 4) return;
			if(waves[0] == WaveType::Long && waves[1] == WaveType::Long && waves[2] == WaveType::Long && waves[3] == WaveType::Long) {
				push_symbol(SymbolType::Zero, 4);
				return;
			}
		break;

		case SlowData:
#define CHECK_RUN(length, type, symbol)	\
			if(waves.size() >= length) {\
				std::size_t c;\
				for(c = 0; c < length; c++) if(waves[c] != type) break;\
				if(c == length) {\
					push_symbol(symbol, length);\
					return;\
				}\
			}

			CHECK_RUN(4, WaveType::Long, SymbolType::Zero);
			CHECK_RUN(8, WaveType::Short, SymbolType::One);
#undef CHECK_RUN
			if(waves.size() < 16) return;	// TODO, maybe: if there are any inconsistencies in the first 8, don't return
		break;

		case Sync: {
			// Sync is 0x16, either encoded fast or slow; i.e. 0 0110 1000 1
			const Pattern slow_sync[] = {
				{WaveType::Long, 8},
				{WaveType::Short, 16},
				{WaveType::Long, 4},
				{WaveType::Short, 8},
				{WaveType::Long, 12},
				{WaveType::Short, 8},
				{WaveType::Unrecognised}
			};
			const Pattern fast_sync[] = {
				{WaveType::Medium,	2},
				{WaveType::Short,	2},
				{WaveType::Medium,	1},
				{WaveType::Short,	1},
				{WaveType::Medium,	3},
				{WaveType::Short,	1},
				{WaveType::Unrecognised}
			};

			std::size_t slow_sync_matching_depth = pattern_matching_depth(waves, slow_sync);
			std::size_t fast_sync_matching_depth = pattern_matching_depth(waves, fast_sync);

			if(slow_sync_matching_depth == 52) {
				push_symbol(SymbolType::FoundSlow, 52);
				return;
			}
			if(fast_sync_matching_depth == 10) {
				push_symbol(SymbolType::FoundFast, 10);
				return;
			}
			if(slow_sync_matching_depth < waves.size() && fast_sync_matching_depth < waves.size()) {
				int least_depth = static_cast<int>(std::min(slow_sync_matching_depth, fast_sync_matching_depth));
				remove_waves(least_depth ? least_depth : 1);
			}

			return;
		}
		break;
	}

	remove_waves(1);
}

std::size_t Parser::pattern_matching_depth(const std::vector<WaveType> &waves, const Pattern *pattern) {
	std::size_t depth = 0;
	int pattern_depth = 0;
	while(depth < waves.size() && pattern->type != WaveType::Unrecognised) {
		if(waves[depth] != pattern->type) break;
		depth++;
		pattern_depth++;
		if(pattern_depth == pattern->count) {
			pattern_depth = 0;
			pattern++;
		}
	}

	return depth;
}
