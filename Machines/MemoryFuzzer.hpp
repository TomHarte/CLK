//
//  MemoryFuzzer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef MemoryFuzzer_hpp
#define MemoryFuzzer_hpp

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Memory {

void Fuzz(uint8_t *buffer, size_t size);
void Fuzz(std::vector<uint8_t> &buffer);

}

#endif /* MemoryFuzzer_hpp */
