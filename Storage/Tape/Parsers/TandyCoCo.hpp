//
//  TandyCoCo.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "TapeParser.hpp"

namespace Storage::Tape::TandyCoCo {

enum class WaveType {
	Short,
	Long,
};

struct Block {
	uint8_t type;
	std::vector<uint8_t> data;
	uint8_t checksum_error;
};

class Parser {
public:
	void find_leadin(Storage::Tape::TapeSerialiser &);
	std::optional<bool> bit(Storage::Tape::TapeSerialiser &);
	std::optional<uint8_t> byte(Storage::Tape::TapeSerialiser &);
	std::optional<Block> block(Storage::Tape::TapeSerialiser &);

private:
	std::optional<WaveType> next(Storage::Tape::TapeSerialiser &);
};

}
