//
//  MemoryFuzzer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef MemoryFuzzer_hpp
#define MemoryFuzzer_hpp

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Memory {

/// Stores @c size random bytes from @c buffer onwards.
void Fuzz(uint8_t *buffer, std::size_t size);

/// Stores @c size random 16-bit words from @c buffer onwards.
void Fuzz(uint16_t *buffer, std::size_t size);

/// Replaces all existing vector contents with random bytes.
void Fuzz(std::vector<uint8_t> &buffer);

}

#endif /* MemoryFuzzer_hpp */
