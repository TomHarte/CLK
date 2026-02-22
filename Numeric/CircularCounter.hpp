//
//  CircularCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <cassert>

namespace Numeric {

template <typename IntT>
constexpr bool is_power_of_two(const IntT value) {
	return !(value & (value - 1));
}

template <typename IntT, IntT limit>
class CircularCounter {
public:
	constexpr CircularCounter() noexcept = default;
	constexpr CircularCounter(const IntT value) noexcept : value_(value) {
		assert(value < limit);
	}

	CircularCounter &operator ++() {
		++value_;

		// Inspection confirms that the compiler does not make the following
		// optimisation automatically.
		if constexpr (is_power_of_two(limit)) {
			value_ &= limit - 1;
		} else {
			if(value_ == limit) {
				value_ = 0;
			}
		}
		return *this;
	}

	CircularCounter operator ++(int) {
		const auto result = *this;
		++*this;
		return result;
	}

	operator IntT() const {
		return value_;
	}

	CircularCounter &operator = (const IntT rhs) {
		value_ = rhs;
		return *this;
	}

private:
	IntT value_{};
};

}
