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

std::optional<File> Parser::file(Storage::Tape::TapeSerialiser &serialiser) {
	std::optional<File> result;

	do {
		// Find next leader.
		const auto leader = block(serialiser);
		if(!leader.has_value()) break;
		if(leader->type != 0) continue;
		if(leader->data.size() != 14) continue;

		// Create file.
		result = File();
		const auto copy_string = [&](auto &target, const size_t offset) {
			std::copy_n(leader->data.data() + offset, sizeof(target) - 1, target);

			auto end = std::find_if(std::rbegin(target) + 1, std::rend(target), [](const char c) {
				return c != ' ';
			});
			*(end - 1) = '\0';
		};

		copy_string(result->name, 0);
		copy_string(result->extension, 8);

		result->type = File::Type(leader->data[11]);
		result->mode = File::Mode((leader->data[12] << 8) | leader->data[13]);

		// Accumulate data for as long as it comes.
		while(true) {
			const auto next = block(serialiser);
			if(next->type != 1) break;
			std::copy(next->data.begin(), next->data.end(), std::back_inserter(result->data));
		}
	} while(false);

	return result;
}

uint8_t Block::check_digit() const {
	return uint8_t(checksum - std::accumulate(data.begin(), data.end(), 0));
}

//
// Notes on file contents, as I figure them out:
//
// File type: 0 = BASIC, 1 = data (saved by BASIC); 2 = binary.
//
// BASIC file modes: 0 = tokenised; 1 = ASCII.
//
// Binary contents:
//
//	byte 0: ???
//	bytes 1 & 2: length of data;
//	bytes 3 & 4: loading address;
//	... data itself ...
//	five more bytes: ???
//
