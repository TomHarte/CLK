//
//  BitReverse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef BitReverse_hpp
#define BitReverse_hpp

#include <cstdint>

namespace Numeric {

/// @returns @c source with the order of its bits reversed. E.g. if @c IntT is @c uint8_t then
/// the reverse of bit pattern abcd efgh is hgfd dcba.
template <typename IntT> constexpr IntT bit_reverse(IntT source);

// The single-byte specialisation uses a lookup table.
template<> constexpr uint8_t bit_reverse<uint8_t>(uint8_t source) {
    struct ReverseTable {
		static constexpr std::array<uint8_t, 256> reverse_table() {
			std::array<uint8_t, 256> map{};
			for(std::size_t c = 0; c < 256; ++c) {
				map[c] = uint8_t(
					((c & 0x80) >> 7) |
					((c & 0x40) >> 5) |
					((c & 0x20) >> 3) |
					((c & 0x10) >> 1) |
					((c & 0x08) << 1) |
					((c & 0x04) << 3) |
					((c & 0x02) << 5) |
					((c & 0x01) << 7)
				);
			}
			return map;
		}
    };

	const std::array<uint8_t, 256> map = ReverseTable::reverse_table();
	return map[source];
}

// All other versions just call the byte-level reverse the appropriate number of times.
template <typename IntT> constexpr IntT bit_reverse(IntT source) {
	IntT result;

	uint8_t *src = reinterpret_cast<uint8_t *>(&source);
	uint8_t *dest = reinterpret_cast<uint8_t *>(&result) + sizeof(result) - 1;
	for(size_t c = 0; c < sizeof(source); c++) {
		*dest = bit_reverse(*src);
		++src;
		--dest;
	}

	return result;
}

}

#endif /* BitReverse_hpp */
