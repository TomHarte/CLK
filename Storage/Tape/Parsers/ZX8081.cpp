//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

using namespace Storage::Tape::ZX8081;

Parser::Parser() : pulse_was_high_(false), pulse_time_(0) {}

void Parser::process_pulse(const Storage::Tape::Tape::Pulse &pulse) {
	pulse_time_ += pulse.length;
	bool pulse_is_high = pulse.type == Storage::Tape::Tape::Pulse::High;

	if(pulse_is_high == pulse_was_high_) return;
	pulse_was_high_ = pulse_is_high;
	post_pulse();
}

void Parser::post_pulse() {
	const float expected_pulse_length = 150.0f / 1000000.0f;
	const float expected_gap_length = 1300.0f / 1000000.0f;
	float pulse_time = pulse_time_.get_float();
	pulse_time_.set_zero();

	if(pulse_time > expected_gap_length * 1.25f) {
		push_wave(WaveType::LongGap);
	}
	else if(pulse_time > expected_pulse_length * 1.25f) {
		push_wave(WaveType::Gap);
	}
	else if(pulse_time >= expected_pulse_length * 0.75f && pulse_time <= expected_pulse_length * 1.25f) {
		push_wave(WaveType::Pulse);
	}
	else {
		push_wave(WaveType::Unrecognised);
	}
}

void Parser::mark_end() {
	// Push the last thing detected, and post an 'unrecognised' to ensure
	// the queue empties out.
	post_pulse();
	push_wave(WaveType::Unrecognised);
}

void Parser::inspect_waves(const std::vector<WaveType> &waves) {
	// A long gap is a file gap.
	if(waves[0] == WaveType::LongGap) {
		push_symbol(SymbolType::FileGap, 1);
		return;
	}

	if(waves.size() >= 9) {
		size_t wave_offset = 0;
		// If the very first thing is a gap, swallow it.
		if(waves[0] == WaveType::Gap) {
			wave_offset = 1;
		}

		// Count the number of pulses at the start of this vector
		size_t number_of_pulses = 0;
		while(number_of_pulses + wave_offset < waves.size() && waves[number_of_pulses + wave_offset] == WaveType::Pulse) {
			number_of_pulses++;
		}

		// If those pulses were followed by a gap then they might be
		// a recognised symbol.
		if(number_of_pulses > 17 || number_of_pulses < 7) {
			push_symbol(SymbolType::Unrecognised, 1);
		}
		else if(number_of_pulses + wave_offset < waves.size() &&
			(waves[number_of_pulses + wave_offset] == WaveType::LongGap || waves[number_of_pulses + wave_offset] == WaveType::Gap)) {
			// A 1 is 18 up/down waves, a 0 is 8. But the final down will be indistinguishable from
			// the gap that follows the bit due to the simplified "high is high, everything else is low"
			// logic applied to pulse detection. So those two things will merge. Meaning we're looking for
			// 17 and/or 7 pulses.
			size_t gaps_to_swallow = wave_offset + ((waves[number_of_pulses + wave_offset] == WaveType::Gap) ? 1 : 0);
			switch(number_of_pulses) {
				case 18:	case 17:	push_symbol(SymbolType::One, (int)(number_of_pulses + gaps_to_swallow));	break;
				case 8:		case 7:		push_symbol(SymbolType::Zero, (int)(number_of_pulses + gaps_to_swallow));	break;
				default:	push_symbol(SymbolType::Unrecognised, 1);			break;
			}
		}
	}
}

int Parser::get_next_byte(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	int c = 8;
	int result = 0;
	while(c--) {
		if(is_at_end(tape)) return -1;
		SymbolType symbol = get_next_symbol(tape);
		if(symbol != SymbolType::One && symbol != SymbolType::Zero) {
			return_symbol(symbol);
			return -1;
		}
		result = (result << 1) | (symbol == SymbolType::One ? 1 : 0);
	}
	return result;
}

std::shared_ptr<std::vector<uint8_t>> Parser::get_next_file_data(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	if(is_at_end(tape)) return nullptr;
	SymbolType symbol = get_next_symbol(tape);
	while((symbol == SymbolType::FileGap || symbol == SymbolType::Unrecognised) && !is_at_end(tape)) {
		symbol = get_next_symbol(tape);
	}
	if(is_at_end(tape)) return nullptr;
	return_symbol(symbol);

	std::shared_ptr<std::vector<uint8_t>> result(new std::vector<uint8_t>);
	int byte;
	while(!is_at_end(tape)) {
		byte = get_next_byte(tape);
		if(byte == -1) return result;
		result->push_back((uint8_t)byte);
	}
	return result;
}

std::shared_ptr<Storage::Data::ZX8081::File> Parser::get_next_file(const std::shared_ptr<Storage::Tape::Tape> &tape) {
	std::shared_ptr<std::vector<uint8_t>> file_data = get_next_file_data(tape);
	if(!file_data) return nullptr;
	return Storage::Data::ZX8081::FileFromData(*file_data);
}
