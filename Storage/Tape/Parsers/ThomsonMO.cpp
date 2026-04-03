//
//  ThomsonMO.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "ThomsonMO.hpp"

#include <numeric>

using namespace Storage::Tape::Thomson::MO;

void Parser::seed_level(const Pulse::Type level) {
	last_type_ = level;
}

std::optional<bool> Parser::bit(Storage::Tape::TapeSerialiser &serialiser) {
	Pulse pulse;

	// Find next transition.
	while(!serialiser.is_at_end()) {
		pulse = serialiser.next_pulse();
		if(pulse.type != last_type_ && pulse.type != Pulse::Type::Zero) {
			break;
		}
	}
	if(serialiser.is_at_end()) return std::nullopt;

	// Advance at least 555µs and sample again.
	float time = 0.0f;
	while(!serialiser.is_at_end()) {
		time += pulse.length.get<float>();
		if(time >= 0.000'555) break;
		pulse = serialiser.next_pulse();
	}
	if(serialiser.is_at_end()) return std::nullopt;

	const bool result = pulse.type == last_type_;
	last_type_ = pulse.type;
	return result;
}

std::optional<uint8_t> Parser::byte(Storage::Tape::TapeSerialiser &serialiser) {
	uint8_t result = 0;

	for(int c = 0; c < 8; c++) {
		const auto next = bit(serialiser);
		if(!next) return std::nullopt;
		result = uint8_t((result << 1) | *next);
	}

	return result;
}

std::optional<Block> Parser::block(Storage::Tape::TapeSerialiser &serialiser) {
	// Look for a leader of 01s, then align for bytes on a 0x3c5a.
	uint32_t bits = 0;
	while(true) {
		const auto next = bit(serialiser);
		if(!next) return std::nullopt;

		bits = uint32_t((bits << 1) | *next);
		if(bits == 0x01013c5a) break;	// i.e. two bytes of lead-in, then the magic constant.
	}

	// Read type and length, seed checksum.
	Block result;

	const auto type = byte(serialiser);
	if(!type) return std::nullopt;
	result.type = *type;

	const auto length = byte(serialiser);
	if(!length) return std::nullopt;
	result.data.resize(uint8_t(*length - 2));	// Length includes: (i) itself; and (ii) the checksum.

	uint8_t checksum = 0;
	for(auto &target: result.data) {
		const auto next = byte(serialiser);
		if(!next) return std::nullopt;
		target = *next;
		checksum += *next;
	}

	const auto trailer = byte(serialiser);
	if(!trailer) return std::nullopt;
	checksum += *trailer;
	result.checksum = checksum;

	return result;
}

uint8_t Block::check_digit() const {
	return uint8_t(checksum - std::accumulate(data.begin(), data.end(), 0));
}
