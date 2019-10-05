//
//  MemoryPacker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MemoryPacker_hpp
#define MemoryPacker_hpp

#include <cstdint>
#include <vector>

namespace Memory {

/*!
	Copies the bytes from @c source into @c target, interpreting them
	as big-endian 16-bit data.
*/
void PackBigEndian16(const std::vector<uint8_t> &source, uint16_t *target);

/*!
	Copies the bytes from @c source into @c target, interpreting them
	as big-endian 16-bit data. @c target will be resized to the proper size
	exactly to contain the contents of @c source.
*/
void PackBigEndian16(const std::vector<uint8_t> &source, std::vector<uint16_t> &target);

}
#endif /* MemoryPacker_hpp */
