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

	uint8_t type = 0;
	uint16_t mode = 0;

	std::vector<uint8_t> data;
	bool checksums_valid = true;
};

class Parser {
public:
	std::optional<bool> bit(Storage::Tape::TapeSerialiser &);
	std::optional<uint8_t> byte(Storage::Tape::TapeSerialiser &);
	std::optional<Block> block(Storage::Tape::TapeSerialiser &);
	std::optional<File> file(Storage::Tape::TapeSerialiser &);

	void seed_level(Pulse::Type);

private:
	Pulse::Type last_type_ = Pulse::Low;
};

}
