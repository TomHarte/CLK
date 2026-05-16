//
//  TandyCoCo.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "TandyCoCo.hpp"

using namespace Storage::Tape::TandyCoCo;

std::optional<WaveType> Parser::next(Storage::Tape::TapeSerialiser &serialiser) {
	const auto pulse = serialiser.next_pulse();

	const auto length = pulse.length.as<float>();
	if(length < 0.75f / 4800.0f || length > 3.0f / 4800.0f) {
		return std::nullopt;
	}

	return length >= 1.5f / 4800.0f ? WaveType::Long : WaveType::Short;
}

void Parser::find_leadin(Storage::Tape::TapeSerialiser &serialiser) {
	// Lead-in tone is repeated instances of 0x55, so look for a four-length
	// short, short, long, long pattern.
	WaveType waves[4];
	size_t wave_pointer = 0;

	while(!serialiser.is_at_end()) {
		const auto next_wave = next(serialiser);
		if(!next_wave.has_value()) {
			wave_pointer = 0;
			continue;
		}

		if(wave_pointer < 3) {
			waves[wave_pointer++] = *next_wave;
			if(wave_pointer != 3) continue;
		} else {
			waves[0] = waves[1];
			waves[1] = waves[2];
			waves[2] = waves[3];
			waves[3] = *next_wave;
		}

		if(
			waves[0] == WaveType::Short &&
			waves[1] == WaveType::Short &&
			waves[2] == WaveType::Long &&
			waves[3] == WaveType::Long
		) {
			return;
		}
	}
}

std::optional<bool> Parser::bit(Storage::Tape::TapeSerialiser &serialiser) {
	const auto next1 = next(serialiser);
	const auto next2 = next(serialiser);

	if(!next1.has_value() || !next2.has_value()) {
		return std::nullopt;
	}
	if(*next1 != *next2) {
		return std::nullopt;
	}
	return *next1 == WaveType::Short;
}

std::optional<uint8_t> Parser::byte(Storage::Tape::TapeSerialiser &serialiser) {
	uint8_t result = 0;

	for(int c = 0; c < 8; c++) {
		const auto b = bit(serialiser);
		if(!b.has_value()) return std::nullopt;

		result = (result >> 1) | (*b ? 0x80 : 0x00);
	}

	return result;
}

std::optional<Block> Parser::block(Storage::Tape::TapeSerialiser &serialiser) {
	find_leadin(serialiser);

	uint16_t sync = 0;
	while(sync != 0x3c55) {
		const auto b = bit(serialiser);
		if(!b.has_value()) return std::nullopt;

		sync = (sync >> 1) | (*b ? 0x8000 : 0x000);
	}

	const auto type = byte(serialiser);
	const auto length = byte(serialiser);
	if(!type || !length) return std::nullopt;

	Block block;
	block.type = *type;
	block.data.reserve(*length);
	for(int c = 0; c < *length; c++) {
		const auto next_byte = byte(serialiser);
		if(!next_byte.has_value()) return std::nullopt;
		block.data.push_back(*next_byte);
	}

	const auto expected_checksum = static_cast<uint8_t>(
		*length +
		std::accumulate(block.data.begin(), block.data.end(), 0)
	);
	const auto checksum = byte(serialiser);
	if(!checksum.has_value()) return std::nullopt;

	block.checksum_error = uint8_t(*checksum - expected_checksum);
	return block;
}
