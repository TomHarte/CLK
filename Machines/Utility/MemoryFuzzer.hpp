//
//  MemoryFuzzer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Memory {

/// Stores @c size random bytes from @c buffer onwards.
void Fuzz(uint8_t *buffer, std::size_t size);

/// Stores @c size random 16-bit words from @c buffer onwards.
void Fuzz(uint16_t *buffer, std::size_t size);

/// Replaces all existing vector or array contents with random bytes.
// TODO: come up with appropriate requires clause.
template <typename T> void Fuzz(T &buffer) {
	Fuzz(reinterpret_cast<uint8_t *>(buffer.data()), buffer.size() * sizeof(typename T::value_type));
}

/// Fills a C-style array with random bytes.
template <typename IntT, size_t n> void Fuzz(IntT (& array)[n]) {
	Fuzz(reinterpret_cast<uint8_t *>(array), n * sizeof(IntT));
}

}
