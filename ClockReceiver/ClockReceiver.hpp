//
//  ClockReceiver.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "ForceInline.hpp"

#include <algorithm>
#include <concepts>
#include <cassert>
#include <cstdint>
#include <limits>

constexpr bool is_power_of_two(const int v) {
	return !(v & (v - 1));
}

/*!
	Provides a class that wraps a plain int, providing most of the basic arithmetic and
	Boolean operators, but forcing callers and receivers to be explicit as to usage.
*/
template <int ClockDenominator>
requires (is_power_of_two(ClockDenominator))
class Clocks {
public:
	static constexpr int Denominator = ClockDenominator;
	using IntType = int64_t;

	constexpr Clocks(const IntType rhs) noexcept : length_(rhs) {}
	constexpr Clocks() noexcept : length_(0) {}

	// Assignments are implemented anywhere they can't lose data.
	template <typename SourceClocks>
	requires (SourceClocks::Denominator <= Denominator)
	constexpr Clocks(const SourceClocks rhs) noexcept {
		*this = rhs.template reduce<Clocks>();
	}

	Clocks operator +=(const Clocks rhs)	{	length_ += rhs.length_;	return *this;	}
	Clocks operator -=(const Clocks rhs)	{	length_ -= rhs.length_;	return *this;	}
	Clocks operator ++() 					{	++length_;	return *this;	}
	Clocks operator --()					{	--length_;	return *this;	}

	Clocks operator ++(int) {
		const Clocks result = *this;
		++length_;
		return result;
	}
	Clocks operator --(int) {
		const Clocks result = *this;
		--length_;
		return result;
	}

	Clocks operator *=(const Clocks rhs) 	{	*this = Clocks(length_ * rhs.length_);	return *this;	}
	Clocks operator /=(const Clocks rhs) 	{	*this = Clocks(length_ / rhs.length_);	return *this;	}
	Clocks operator %=(const Clocks rhs) 	{	*this = Clocks(length_ % rhs.length_);	return *this;	}
	Clocks operator &=(const Clocks rhs) 	{	*this = Clocks(length_ & rhs.length_);	return *this;	}

	constexpr Clocks operator +(const Clocks rhs) const		{	return Clocks(length_ + rhs.length_);	}
	constexpr Clocks operator -(const Clocks rhs) const		{	return Clocks(length_ - rhs.length_);	}

	constexpr Clocks operator *(const Clocks rhs) const		{	return Clocks(length_ * rhs.length_);	}
	constexpr Clocks operator /(const Clocks rhs) const		{	return Clocks(length_ / rhs.length_);	}

	constexpr Clocks operator %(const Clocks rhs) const		{	return Clocks(length_ % rhs.length_);	}
	constexpr Clocks operator &(const Clocks rhs) const		{	return Clocks(length_ & rhs.length_);	}

	constexpr Clocks operator -() const						{	return Clocks(-length_);				}

	auto operator <=>(const Clocks &) const = default;

	constexpr bool operator !() const					{	return !length_;				}
	// bool operator () is not supported because it offers an implicit cast to int,
	// which is prone silently to permit misuse.

	/// @returns The underlying int, converted to a numeric type of your choosing, clamped to that type's range.
	template <typename Type = IntType>
	requires std::integral<Type> || std::floating_point<Type>
	constexpr Type as() const {
		const auto value = get();

		if constexpr (sizeof(Type) == sizeof(IntType) && std::is_integral_v<Type>) {
			if constexpr (std::is_same_v<Type, IntType>) {
				return value;
			} else if constexpr (std::is_signed_v<Type>) {
				// Both integers are the same size, but a signed result is being asked for
				// from an unsigned original.
				return value > Type(std::numeric_limits<Type>::max()) ?
					Type(std::numeric_limits<Type>::max()) : Type(value);
			} else {
				// An unsigned result is being asked for from a signed original.
				return value < 0 ? 0 : Type(value);
			}
		}

		return Type(std::clamp(length_, low<Type>, high<Type>));
	}

	/// @returns The underlying int, in its native form, potentially scaled upward.
	template <int GetDenominator = Denominator>
	requires (GetDenominator >= Denominator)
	constexpr IntType get() const {
		return length_ << (GetDenominator - Denominator);
	}

	/// @returns This value, optionally converted to another time base. Potentially loses precision.
	template <typename TargetClocks>
	constexpr TargetClocks reduce() const {
		if constexpr (TargetClocks::Denominator >= Denominator) {
			return TargetClocks(length_ << (TargetClocks::Denominator - Denominator));
		} else {
			return TargetClocks(length_ >> (Denominator - TargetClocks::Denominator));
		}
	}

	// operator int() is deliberately not provided, to avoid accidental subtitution of
	// classes that use this template.

	/*!
		Caculates `*this / divisor`, converting that to `DestinationClocks`.
		Sets `*this = *this % divisor`.
	*/
	template <typename DestinationClocks = Clocks>
	requires (DestinationClocks::Denominator >= Denominator)
	DestinationClocks divide(const Clocks divisor) {
		Clocks result;
		result.length_ = length_ / divisor.length_;
		length_ %= divisor.length_;
		return reduce<DestinationClocks>();
	}

	/*!
		Extracts a whole number of `DestinationClock`s from `*this`.
		Leaves the residue here.
	*/
	template <typename DestinationClocks = Clocks>
	DestinationClocks flush() {
		if constexpr (DestinationClocks::Denominator >= Denominator) {
			const auto result = DestinationClocks(get<DestinationClocks::Denominator>());
			length_ = 0;
			return result;
		} else {
			static constexpr int ShiftRight = Denominator - DestinationClocks::Denominator;
			static constexpr IntType ResidueMask = (1 << ShiftRight) - 1;
			const auto result = reduce<DestinationClocks>();
			length_ &= ResidueMask;
			return result;
		}
	}

	static Clocks max() { return Clocks(std::numeric_limits<IntType>::max()); }
	static Clocks min() { return Clocks(std::numeric_limits<IntType>::min()); }

private:
	IntType length_;

	template <typename Type>
	static consteval bool can_represent(const Type x) {
		return std::numeric_limits<IntType>::min() <= x && std::numeric_limits<IntType>::max() >= x;
	}

	template<typename Type>
	static constexpr IntType low =
		can_represent(std::numeric_limits<Type>::min()) ?
			IntType(std::numeric_limits<Type>::min()) : std::numeric_limits<IntType>::min();

	template<typename Type>
	static constexpr IntType high =
		can_represent(std::numeric_limits<Type>::max()) ?
			IntType(std::numeric_limits<Type>::max()) : std::numeric_limits<IntType>::max();
};

/// Reasons Clocks into being a count of querter cycles, building half- and whole-cycles from there.
using Cycles = Clocks<1>;
using HalfCycles = Clocks<2>;
using QuarterCycles = Clocks<4>;

/*!
	Provides automated boilerplate for connecting an owner that works in one clock base to a receiver that works in another.
*/
template <typename TargetT, typename SourceClocks, typename DestinationClocks>
class ConvertedClockReceiver: public TargetT {
public:
	using TargetT::TargetT;

	void run_for(const SourceClocks duration) {
		source_ += duration;
		TargetT::run_for(source_.template flush<DestinationClocks>());
	}

private:
	SourceClocks source_;
};
