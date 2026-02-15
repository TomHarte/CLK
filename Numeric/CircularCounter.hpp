//
//  CircularCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include <cassert>

namespace Numeric {

template <typename IntT, IntT limit>
class CircularCounter {
public:
	constexpr CircularCounter() noexcept = default;
	constexpr CircularCounter(const IntT value) noexcept : value_(value) {
		assert(value < limit);
	}

	CircularCounter &operator ++() {
		++value_;
		if(value_ == limit) {
			value_ = 0;
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
