//
//  MemoryFuzzer.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "MemoryFuzzer.hpp"

#include <cstdlib>

void Memory::Fuzz(uint8_t *buffer, std::size_t size) {
	const unsigned int divider = (unsigned(RAND_MAX) + 1) / 256;
	unsigned int shift = 1, value = 1;
	while(value < divider) {
		value <<= 1;
		++shift;
	}

	for(size_t c = 0; c < size; c++) {
		buffer[c] = uint8_t(std::rand() >> shift);
	}
}

void Memory::Fuzz(uint16_t *buffer, std::size_t size) {
	Fuzz(reinterpret_cast<uint8_t *>(buffer), size * sizeof(uint16_t));
}
