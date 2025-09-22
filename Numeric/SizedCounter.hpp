//
//  SizedCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Sizes.hpp"

namespace Numeric {

/*!
	Provides a counter that is strictly limited to the requested of bits but attempts otherwise
	to act like a standard C++ numeric type.
*/
template <int bits>
struct SizedCounter {
	using IntT = MinIntForValue<1 << bits>::type;
	inline static constexpr IntT Mask = (1 << bits) - 1;

	constexpr SizedCounter(const IntT start_value) noexcept : counter_(start_value & Mask) {}
	SizedCounter() = default;

	IntT get() const {
		return counter_;
	}

	SizedCounter operator+(const SizedCounter offset) {
		return SizedCounter<bits>(counter_ + offset.counter_);
	}

	SizedCounter &operator++(int) {
		(*this) ++;
		return *this;
	}

	SizedCounter &operator++() {
		counter_ = (counter_ + 1) & Mask;
		return *this;
	}

	SizedCounter &operator+=(const IntT rhs) {
		counter_ = (counter_ + rhs) & Mask;
		return *this;
	}

	bool operator!() const {
		return !counter_;
	}

	auto operator <=>(const SizedCounter &) const = default;

	/// Replaces the bits in the range [begin, end) with those in the low-order bits of @c vlaue.
	template <int begin, int end>
	void load(const MinIntForValue<1 << (end - begin)>::type value) {
		const auto mask = (1 << end) - (1 << begin);
		counter_ &= ~mask;
		counter_ |= IntT((value << begin) & mask);
	}

	template <int begin, typename IntT>
	void load(const IntT value) {
		load<begin, begin + sizeof(IntT)*8>(value);
	}

private:
	IntT counter_{};
};

}
