//
//  MemoryPacker.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "MemoryPacker.hpp"

void Memory::PackBigEndian16(const std::vector<uint8_t> &source, uint16_t *target) {
	for(size_t c = 0; c < source.size(); c += 2) {
		target[c >> 1] = uint16_t(source[c] << 8) | uint16_t(source[c+1]);
	}
}

void Memory::PackBigEndian16(const std::vector<uint8_t> &source, uint8_t *target) {
	PackBigEndian16(source, reinterpret_cast<uint16_t *>(target));
}

void Memory::PackBigEndian16(const std::vector<uint8_t> &source, std::vector<uint16_t> &target) {
	target.resize(source.size() >> 1);
	PackBigEndian16(source, target.data());
}

void Memory::PackBigEndian16(const std::vector<uint8_t> &source, std::vector<uint8_t> &target) {
	target.resize(source.size());
	PackBigEndian16(source, target.data());
}
