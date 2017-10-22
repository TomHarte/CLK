//
//  MemoryFuzzer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "MemoryFuzzer.hpp"

#include <cstdlib>

void Memory::Fuzz(uint8_t *buffer, size_t size) {
	unsigned int divider = (static_cast<unsigned int>(RAND_MAX) + 1) / 256;
	unsigned int shift = 1, value = 1;
	while(value < divider) {
		value <<= 1;
		shift++;
	}

	for(size_t c = 0; c < size; c++) {
		buffer[c] = static_cast<uint8_t>(std::rand() >> shift);
	}
}

void Memory::Fuzz(std::vector<uint8_t> &buffer) {
	Fuzz(buffer.data(), buffer.size());
}
