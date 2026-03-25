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
	bool checksum_valid = true;
};

class Parser {
public:
	std::optional<bool> bit(Storage::Tape::TapeSerialiser &);
	std::optional<uint8_t> byte(Storage::Tape::TapeSerialiser &);
	std::optional<Block> block(Storage::Tape::TapeSerialiser &);

private:
	Pulse::Type last_type_ = Pulse::Low;
};

}
