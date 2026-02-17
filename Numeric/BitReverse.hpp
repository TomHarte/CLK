//
//  BitReverse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Sizes.hpp"

#include <concepts>
#include <cstdint>

namespace Numeric {

/// @returns @c source with the order of its bits reversed. E.g. if @c IntT is @c uint8_t then
/// the reverse of bit pattern abcd efgh is hgfd dcba.
template <typename IntT> constexpr IntT bit_reverse(IntT source);

// Single-byte specialisation.
template<> constexpr uint8_t bit_reverse<uint8_t>(uint8_t source) {
	source = uint8_t(((source & 0b1111'0000) >> 4) | ((source & 0b0000'1111) << 4));
	source = uint8_t(((source & 0b1100'1100) >> 2) | ((source & 0b0011'0011) << 2));
	source = uint8_t(((source & 0b1010'1010) >> 1) | ((source & 0b0101'0101) << 1));
	return source;
}

// All other versions recursively subdivide.
template <typename IntT>
requires std::same_as<IntT, uint16_t> || std::same_as<IntT, uint32_t> || std::same_as<IntT, uint64_t>
constexpr IntT bit_reverse(const IntT source) {
	constexpr auto HalfSize = sizeof(IntT) * 4;
	using HalfIntT = uint_t<HalfSize>;

	return IntT(
		IntT(bit_reverse(HalfIntT(source >> HalfSize))) |
		IntT(bit_reverse(HalfIntT(source))) << HalfSize
	);
}

}
