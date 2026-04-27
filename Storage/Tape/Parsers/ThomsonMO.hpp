//
//  ThomsonMO.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "TapeParser.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace Storage::Tape::Thomson::MO {

struct Block {
	uint8_t type;
	std::vector<uint8_t> data;

	uint8_t checksum = 0;
	bool checksum_valid() const {
		return checksum == 0;
	}
	uint8_t check_digit() const;
};

struct File {
	char name[9]{};
	char extension[4]{};
	std::string filename() const {
		return std::string(name) + '.' + extension;
	}

	enum class Type: uint8_t {
		BASIC = 0,
		Data = 1,
		Binary = 2,
	};
	Type type{};

	enum class Mode: uint16_t {
		// Modes that apply when type is BASIC.
		BASICTokenised = 0,
		BASICASCII = 1,
	};
	Mode mode{};

	std::vector<uint8_t> data;
	bool checksums_valid = true;
};

class Parser {
public:
	int calibrated_sample_delay(Storage::Tape::TapeSerialiser &);
	std::optional<bool> bit(Storage::Tape::TapeSerialiser &, int sample_delay_us = 555);

	std::optional<uint8_t> byte(Storage::Tape::TapeSerialiser &, int sample_delay_us = 555);
	std::optional<Block> block(Storage::Tape::TapeSerialiser &);
	std::optional<File> file(Storage::Tape::TapeSerialiser &);

	void seed_level(Pulse::Type);

private:
	Pulse::Type last_type_ = Pulse::Low;
};

}
