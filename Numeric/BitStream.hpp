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
#include <functional>

namespace Numeric {

template <typename MaxIntT, bool lsb_first>
class BitStream {
public:
	BitStream(std::function<uint8_t(void)> next_byte) : next_byte_(next_byte) {}

	template <size_t bits = 0>
	MaxIntT next(size_t rbits = 0) {
		const size_t required = bits ? bits : rbits;
		while(enqueued_ < required) {
			uint8_t next = next_byte_();
			if constexpr (lsb_first) {
				next = bit_reverse(next);
			}

			input_ |= ShiftT(next) << (BitSize - 8 - enqueued_);
			enqueued_ += 8;
		}

		const auto result = MaxIntT(input_ >> (BitSize - required));
		input_ <<= required;
		enqueued_ -= required;
		return result;
	}

private:
	std::function<uint8_t(void)> next_byte_;

	size_t enqueued_{};

	using ShiftT = uint_t<sizeof(MaxIntT) * 8 * 2>;
	ShiftT input_{};
	static constexpr size_t BitSize = sizeof(ShiftT) * 8;
};

}
