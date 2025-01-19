//
//  ZX8081.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "ZX8081.hpp"

using namespace Storage::Tape::ZX8081;

Parser::Parser() : pulse_was_high_(false), pulse_time_(0) {}

void Parser::process_pulse(const Storage::Tape::Pulse &pulse) {
	// If this is anything other than a transition from low to high, just add it to the
	// count of time.
	const bool pulse_is_high = pulse.type == Storage::Tape::Pulse::High;
	const bool pulse_did_change = pulse_is_high != pulse_was_high_;
	pulse_was_high_ = pulse_is_high;
	if(!pulse_did_change || !pulse_is_high) {
		pulse_time_ += pulse.length;
		return;
	}

	// Otherwise post a new pulse.
	post_pulse();
	pulse_time_ = pulse.length;
}

void Parser::post_pulse() {
	constexpr float expected_pulse_length = 300.0f / 1000000.0f;
	constexpr float expected_gap_length = 1300.0f / 1000000.0f;
	const auto pulse_time = pulse_time_.get<float>();

	if(pulse_time > expected_gap_length * 1.25f) {
		push_wave(WaveType::LongGap);
	} else if(pulse_time > expected_pulse_length * 1.25f) {
		push_wave(WaveType::Gap);
	} else if(pulse_time >= expected_pulse_length * 0.75f && pulse_time <= expected_pulse_length * 1.25f) {
		push_wave(WaveType::Pulse);
	} else {
		push_wave(WaveType::Unrecognised);
	}
}

void Parser::mark_end() {
	// Post a long gap to cap any bit that's in the process of recognition.
	push_wave(WaveType::LongGap);
}

void Parser::inspect_waves(const std::vector<WaveType> &waves) {
	// A long gap is a file gap.
	if(waves[0] == WaveType::LongGap) {
		push_symbol(SymbolType::FileGap, 1);
		return;
	}

	if(waves[0] == WaveType::Unrecognised) {
		push_symbol(SymbolType::Unrecognised, 1);
		return;
	}

	if(waves.size() >= 4) {
		std::size_t wave_offset = 0;
		// If the very first thing is a gap, swallow it.
		if(waves[0] == WaveType::Gap) {
			wave_offset = 1;
		}

		// Count the number of pulses at the start of this vector
		std::size_t number_of_pulses = 0;
		while(number_of_pulses + wave_offset < waves.size() && waves[number_of_pulses + wave_offset] == WaveType::Pulse) {
			number_of_pulses++;
		}

		// If those pulses were followed by something not recognised as a pulse, check for a bit
		if(number_of_pulses + wave_offset < waves.size()) {
			// A 1 is 9 waves, a 0 is 4. Counting upward zero transitions, the first in either group will
			// act simply to terminate the gap beforehand and won't be logged as a pulse. So counts to
			// check are 8 and 3.
			std::size_t gaps_to_swallow = wave_offset + ((waves[number_of_pulses + wave_offset] == WaveType::Gap) ? 1 : 0);
			switch(number_of_pulses) {
				case 8:		push_symbol(SymbolType::One, int(number_of_pulses + gaps_to_swallow));		break;
				case 3:		push_symbol(SymbolType::Zero, int(number_of_pulses + gaps_to_swallow));		break;
				default:	push_symbol(SymbolType::Unrecognised, 1);									break;
			}
		}
	}
}

int Parser::get_next_byte(Storage::Tape::TapeSerialiser &serialiser) {
	int c = 8;
	int result = 0;
	while(c) {
		if(is_at_end(serialiser)) return -1;

		SymbolType symbol = get_next_symbol(serialiser);
		if(symbol != SymbolType::One && symbol != SymbolType::Zero) {
			if(c == 8) continue;
			return_symbol(symbol);
			return -1;
		}

		result = (result << 1) | (symbol == SymbolType::One ? 1 : 0);
		c--;
	}
	return result;
}

std::optional<std::vector<uint8_t>> Parser::get_next_file_data(Storage::Tape::TapeSerialiser &serialiser) {
	if(is_at_end(serialiser)) return std::nullopt;
	SymbolType symbol = get_next_symbol(serialiser);
	if(symbol != SymbolType::FileGap) {
		return std::nullopt;
	}
	while((symbol == SymbolType::FileGap || symbol == SymbolType::Unrecognised) && !is_at_end(serialiser)) {
		symbol = get_next_symbol(serialiser);
	}
	if(is_at_end(serialiser)) return std::nullopt;
	return_symbol(symbol);

	std::vector<uint8_t> result;
	int byte;
	while(!is_at_end(serialiser)) {
		byte = get_next_byte(serialiser);
		if(byte == -1) return result;
		result.push_back(uint8_t(byte));
	}
	return result;
}

std::optional<Storage::Data::ZX8081::File> Parser::get_next_file(Storage::Tape::TapeSerialiser &serialiser) {
	const auto file_data = get_next_file_data(serialiser);
	if(!file_data) {
		return std::nullopt;
	}
	return Storage::Data::ZX8081::FileFromData(*file_data);
}
