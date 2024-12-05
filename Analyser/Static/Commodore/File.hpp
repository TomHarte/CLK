//
//  File.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Analyser::Static::Commodore {

struct File {
	std::wstring name;
	std::vector<uint8_t> raw_name;
	uint16_t starting_address;
	uint16_t ending_address;
	bool is_locked = false;
	bool is_closed = false;
	enum {
		RelocatableProgram,
		NonRelocatableProgram,
		DataSequence,
		User,
		Relative
	} type;
	std::vector<uint8_t> data;
};

}
