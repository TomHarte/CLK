//
//  SizedCounter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/09/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Sizes.hpp"

#include <concepts>

namespace Numeric {

/*!
	Provides a counter that is strictly limited to the requested of bits but attempts otherwise
	to act like a standard C++ numeric type.
*/
template <int bits>
struct SizedInt {
	using IntT = MinIntForValue<1 << bits>::type;
	inline static constexpr IntT Mask = (1 << bits) - 1;

	template <std::integral ConstructionT>
	constexpr SizedInt(const ConstructionT start_value) noexcept : value_(IntT(start_value & Mask)) {}

	SizedInt() = default;

	template <int begin = 0>
	IntT get() const {
		return value_ >> begin;
	}

	SizedInt operator +(const SizedInt offset) const {	return SizedInt<bits>(value_ + offset.value_); }
	SizedInt operator -(const SizedInt offset) const {	return SizedInt<bits>(value_ - offset.value_); }
	SizedInt operator &(const SizedInt offset) const {	return SizedInt<bits>(value_ & offset.value_); }
	SizedInt operator |(const SizedInt offset) const {	return SizedInt<bits>(value_ | offset.value_); }
	SizedInt operator ^(const SizedInt offset) const {	return SizedInt<bits>(value_ ^ offset.value_); }
	SizedInt operator >>(const int shift) const {	return SizedInt<bits>(value_ >> shift);	}
	SizedInt operator <<(const int shift) const {	return SizedInt<bits>(value_ << shift);	}

	SizedInt &operator &=(const SizedInt offset) {
		value_ &= offset.value_;
		return *this;
	}
	SizedInt &operator |=(const SizedInt offset) {
		value_ |= offset.value_;
		return *this;
	}
	SizedInt &operator ^=(const SizedInt offset) {
		value_ ^= offset.value_;
		return *this;
	}

	SizedInt &operator <<=(const int shift) {
		value_ = (value_ << shift) & Mask;
		return *this;
	}

	SizedInt &operator >>=(const int shift) {
		value_ >>= shift;
		return *this;
	}

	SizedInt &operator ++(int) {
		++(*this);
		return *this;
	}

	SizedInt &operator ++() {
		value_ = (value_ + 1) & Mask;
		return *this;
	}

	SizedInt &operator +=(const IntT rhs) {
		value_ = (value_ + rhs) & Mask;
		return *this;
	}

	bool operator!() const {
		return !value_;
	}

	auto operator <=>(const SizedInt &) const = default;

	/// Replaces the bits in the range [begin, end) with those in the low-order bits of @c vlaue.
	template <int begin, int end>
	void load(const MinIntForValue<1 << (end - begin)>::type value) {
		const auto mask = (1 << end) - (1 << begin);
		value_ &= ~mask;
		value_ |= IntT((value << begin) & mask);
	}

	template <int begin, typename IntT>
	void load(const IntT value) {
		load<begin, begin + sizeof(IntT)*8>(value);
	}

	template <int index>
	requires (index < bits)
	bool bit() const {
		return value_ & (1 << index);
	}

private:
	IntT value_{};
};

}
