//
//  CircularCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <cassert>
#include <concepts>

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

	template <std::integral CastT>
	explicit operator CastT() const {
		return CastT(value_);
	}

	CircularCounter &operator = (const IntT rhs) {
		value_ = rhs;
		return *this;
	}

	template <typename RHSIntT, RHSIntT rhs_limit>
	bool operator ==(const CircularCounter<RHSIntT, rhs_limit> rhs) const {
		if constexpr (std::is_same_v<RHSIntT, IntT>) {
			return value_ == rhs.value_;
		}

		return size_t(value_) == size_t(rhs.value_);
	}

private:
	IntT value_{};
};

}
