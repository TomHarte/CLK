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
struct SizedInt {
	using IntT = MinIntForValue<1 << bits>::type;
	inline static constexpr IntT Mask = (1 << bits) - 1;

	constexpr SizedInt(const IntT start_value) noexcept : counter_(start_value & Mask) {}
	SizedInt() = default;

	IntT get() const {
		return counter_;
	}

	SizedInt operator +(const SizedInt offset) const {	return SizedInt<bits>(counter_ + offset.counter_); }
	SizedInt operator -(const SizedInt offset) const {	return SizedInt<bits>(counter_ - offset.counter_); }
	SizedInt operator &(const SizedInt offset) const {	return SizedInt<bits>(counter_ & offset.counter_); }
	SizedInt operator |(const SizedInt offset) const {	return SizedInt<bits>(counter_ | offset.counter_); }
	SizedInt operator ^(const SizedInt offset) const {	return SizedInt<bits>(counter_ ^ offset.counter_); }
	SizedInt operator >>(const int shift) const {	return SizedInt<bits>(counter_ >> shift);	}
	SizedInt operator <<(const int shift) const {	return SizedInt<bits>(counter_ << shift);	}

	SizedInt &operator &=(const SizedInt offset) {
		counter_ &= offset.counter_;
		return *this;
	}
	SizedInt &operator |=(const SizedInt offset) {
		counter_ |= offset.counter_;
		return *this;
	}
	SizedInt &operator ^=(const SizedInt offset) {
		counter_ ^= offset.counter_;
		return *this;
	}

	SizedInt &operator <<=(const int shift) {
		counter_ = (counter_ << shift) & Mask;
		return *this;
	}

	SizedInt &operator >>=(const int shift) {
		counter_ >>= shift;
		return *this;
	}

	SizedInt &operator ++(int) {
		++(*this);
		return *this;
	}

	SizedInt &operator ++() {
		counter_ = (counter_ + 1) & Mask;
		return *this;
	}

	SizedInt &operator +=(const IntT rhs) {
		counter_ = (counter_ + rhs) & Mask;
		return *this;
	}

	bool operator!() const {
		return !counter_;
	}

	auto operator <=>(const SizedInt &) const = default;

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

	template <int index>
	requires (index < bits)
	bool bit() const {
		return counter_ & (1 << index);
	}

private:
	IntT counter_{};
};

}
