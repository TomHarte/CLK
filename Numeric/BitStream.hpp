//
//  BitStream.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/02/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "BitReverse.hpp"
#include "Sizes.hpp"

#include <cstdint>
#include <functional>

namespace Numeric {

/*!
	Given a means to fetch the next byte from a byte stream, serialises those bytes into a bit stream.

	@c max_bits is the largest number of bits that'll be read at once.
	@c lsb_first if true then the LSB of each byte from the byte stream is the first bit read. Otherwise it's the MSB.
*/
template <int max_bits, bool lsb_first>
class BitStream {
public:
	using IntT = min_int_for_bits_t<max_bits>;

	BitStream(std::function<uint8_t(void)> next_byte) : next_byte_(next_byte) {}

	/// @returns An integer composed of the next n bits of the bitstream where n is:
	/// * the template parameter `bits` if it is non-zero; or
	/// * the function argument `rbits` otherwise.
	/// `rbits` is ignored if `bits` is non-zero.
	template <size_t bits = 0>
	IntT next([[maybe_unused]] const size_t rbits = 0) {
		static_assert(bits <= max_bits);
		static constexpr size_t ShiftBitSize = sizeof(ShiftIntT) * 8;

		const size_t required = bits ? bits : rbits;
		while(enqueued_ < required) {
			uint8_t next = next_byte_();
			if constexpr (lsb_first) {
				next = bit_reverse(next);
			}

			input_ |= ShiftIntT(next) << (ShiftBitSize - 8 - enqueued_);
			enqueued_ += 8;
		}

		const auto result = IntT(input_ >> (ShiftBitSize - required));
		input_ <<= required;
		enqueued_ -= required;
		return result;
	}

private:
	std::function<uint8_t(void)> next_byte_;

	using ShiftIntT = min_int_for_bits_t<max_bits + 7>;
	ShiftIntT input_{};
	size_t enqueued_{};
};

}
