//
//  BitReverse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include <array>
#include <cstdint>

namespace Numeric {
namespace {

constexpr auto reverse_map = [] {
	std::array<uint8_t, 256> table{};
	for(std::size_t c = 0; c < 256; ++c) {
		table[c] = uint8_t(
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
	return table;
} ();

}

/// @returns @c source with the order of its bits reversed. E.g. if @c IntT is @c uint8_t then
/// the reverse of bit pattern abcd efgh is hgfd dcba.
template <typename IntT> constexpr IntT bit_reverse(IntT source);

// The single-byte specialisation uses a lookup table.
template<> constexpr uint8_t bit_reverse<uint8_t>(const uint8_t source) {
	return reverse_map[source];
}

// All other versions recursively subdivide.
template <typename IntT>
constexpr IntT bit_reverse(const IntT source) {
	static_assert(std::is_same_v<IntT, uint16_t> || std::is_same_v<IntT, uint32_t> || std::is_same_v<IntT, uint64_t>);
	using HalfIntT =
		std::conditional_t<std::is_same_v<IntT, uint16_t>, uint8_t,
			std::conditional_t<std::is_same_v<IntT, uint32_t>, uint16_t,
				uint32_t>>;
	constexpr auto HalfShift = sizeof(IntT) * 4;

	return IntT(
		IntT(bit_reverse(HalfIntT(source))) << HalfShift) |
		IntT(bit_reverse(HalfIntT(source >> HalfShift))
	);
}

}
