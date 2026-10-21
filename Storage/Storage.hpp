//
//  Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

namespace Storage {

/*!
	Contains either an absolute time or a time interval, described as a quotient, in terms of a
	clock rate to which the time is relative and its length in cycles based on that clock rate.
*/
struct Time {
	unsigned int length, clock_rate;
	constexpr Time() noexcept : length(0), clock_rate(1) {}

	template <std::integral IntT>
	requires (sizeof(IntT) <= sizeof(int))
	constexpr Time(const IntT value) noexcept : length(unsigned(value)), clock_rate(1) {}

	template <std::integral IntT>
	requires (sizeof(IntT) <= sizeof(int))
	constexpr Time(const IntT length, const IntT clock_rate) noexcept :
		length(unsigned(length)), clock_rate(unsigned(clock_rate)) {}

	Time(const uint64_t length, const uint64_t clock_rate) noexcept {
		install_result(length, clock_rate);
	}
	Time(const float value) noexcept {
		install_float(value);
	}
	static constexpr Time simplified(const unsigned int _length, const unsigned int _clock_rate) {
		const auto gcd = std::gcd(_length, _clock_rate);
		return Time(_length / gcd, _clock_rate / gcd);
	}

	/*!
		Reduces this @c Time to its simplest form; eliminates all common factors from @c length
		and @c clock_rate.
	*/
	void simplify() {
		unsigned int common_divisor = std::gcd(length, clock_rate);
		length /= common_divisor;
		clock_rate /= common_divisor;
	}

	/*!
		@returns the floating point conversion of this @c Time. This will often be less precise.
	*/
	template <typename T> T as() const {
		return T(length) / T(clock_rate);
	}

	auto operator <=> (const Time &rhs) const {
		return uint64_t(rhs.clock_rate) * uint64_t(length) <=> uint64_t(clock_rate) * uint64_t(rhs.length);
	}

	auto operator == (const Time &rhs) const {
		return *this <=> rhs == std::strong_ordering::equal;
	}

	Time operator + (const Time &other) const {
		if(!other.length) return *this;

		uint64_t result_length;
		uint64_t result_clock_rate;
		if(clock_rate == other.clock_rate) {
			result_length = uint64_t(length) + uint64_t(other.length);
			result_clock_rate = clock_rate;
		} else {
			result_length =
				uint64_t(length) * uint64_t(other.clock_rate) + uint64_t(other.length) * uint64_t(clock_rate);
			result_clock_rate = uint64_t(clock_rate) * uint64_t(other.clock_rate);
		}
		return Time(result_length, result_clock_rate);
	}

	Time &operator += (const Time &other) {
		if(!other.length) return *this;
		if(!length) {
			*this = other;
			return *this;
		}

		uint64_t result_length;
		uint64_t result_clock_rate;
		if(clock_rate == other.clock_rate) {
			result_length = uint64_t(length) + uint64_t(other.length);
			result_clock_rate = uint64_t(clock_rate);
		} else {
			result_length =
				uint64_t(length) * uint64_t(other.clock_rate) + uint64_t(other.length) * uint64_t(clock_rate);
			result_clock_rate = uint64_t(clock_rate) * uint64_t(other.clock_rate);
		}
		install_result(result_length, result_clock_rate);
		return *this;
	}

	Time operator - (const Time &other) const {
		if(!other.length) return *this;

		uint64_t result_length;
		uint64_t result_clock_rate;
		if(clock_rate == other.clock_rate) {
			result_length = uint64_t(length) - uint64_t(other.length);
			result_clock_rate = clock_rate;
		} else {
			result_length =
				uint64_t(length) * uint64_t(other.clock_rate) - uint64_t(other.length) * uint64_t(clock_rate);
			result_clock_rate = uint64_t(clock_rate) * uint64_t(other.clock_rate);
		}
		return Time(result_length, result_clock_rate);
	}

	Time operator -= (const Time &other) {
		if(!other.length) return *this;

		uint64_t result_length;
		uint64_t result_clock_rate;
		if(clock_rate == other.clock_rate) {
			result_length = uint64_t(length) - uint64_t(other.length);
			result_clock_rate = uint64_t(clock_rate);
		} else {
			result_length =
				uint64_t(length) * uint64_t(other.clock_rate) - uint64_t(other.length) * uint64_t(clock_rate);
			result_clock_rate = uint64_t(clock_rate) * uint64_t(other.clock_rate);
		}
		install_result(result_length, result_clock_rate);
		return *this;
	}

	Time operator * (const Time &other) const {
		uint64_t result_length = uint64_t(length) * uint64_t(other.length);
		uint64_t result_clock_rate = uint64_t(clock_rate) * uint64_t(other.clock_rate);
		return Time(result_length, result_clock_rate);
	}

	Time &operator *= (const Time &other) {
		uint64_t result_length = uint64_t(length) * uint64_t(other.length);
		uint64_t result_clock_rate = uint64_t(clock_rate) * uint64_t(other.clock_rate);
		install_result(result_length, result_clock_rate);
		return *this;
	}

	Time operator * (unsigned int multiplier) const {
		uint64_t result_length = uint64_t(length) * uint64_t(multiplier);
		uint64_t result_clock_rate = uint64_t(clock_rate);
		return Time(result_length, result_clock_rate);
	}

	Time &operator *= (unsigned int multiplier) {
		uint64_t result_length = uint64_t(length) * uint64_t(multiplier);
		uint64_t result_clock_rate = uint64_t(clock_rate);
		install_result(result_length, result_clock_rate);
		return *this;
	}

	Time operator / (const Time &other) const {
		uint64_t result_length = uint64_t(length) * uint64_t(other.clock_rate);
		uint64_t result_clock_rate = uint64_t(clock_rate) * uint64_t(other.length);
		return Time(result_length, result_clock_rate);
	}

	Time &operator /= (const Time &other) {
		uint64_t result_length = uint64_t(length) * uint64_t(other.clock_rate);
		uint64_t result_clock_rate = uint64_t(clock_rate) * uint64_t(other.length);
		install_result(result_length, result_clock_rate);
		return *this;
	}

	Time operator / (unsigned int divisor) const {
		uint64_t result_length = uint64_t(length);
		uint64_t result_clock_rate = uint64_t(clock_rate) * uint64_t(divisor);
		return Time(result_length, result_clock_rate);
	}

	Time &operator /= (unsigned int divisor) {
		uint64_t result_length = uint64_t(length);
		uint64_t result_clock_rate = uint64_t(clock_rate) * uint64_t(divisor);
		install_result(result_length, result_clock_rate);
		return *this;
	}

	void set_zero() {
		length = 0;
		clock_rate = 1;
	}

	void set_one() {
		length = 1;
		clock_rate = 1;
	}

	static Time max() {
		return Time(std::numeric_limits<unsigned int>::max());
	}

private:
	void install_result(uint64_t long_length, uint64_t long_clock_rate) {
		if(
			long_length <= std::numeric_limits<unsigned int>::max() &&
			long_clock_rate <= std::numeric_limits<unsigned int>::max()
		) {
			length = unsigned(long_length);
			clock_rate = unsigned(long_clock_rate);
			return;
		}

		// TODO: switch to appropriate values if the result is too large or small to fit, even with trimmed accuracy.
		if(!long_length) {
			length = 0;
			clock_rate = 1;
			return;
		}

		while(!(long_length&0xf) && !(long_clock_rate&0xf)) {
			long_length >>= 4;
			long_clock_rate >>= 4;
		}

		while(!(long_length&1) && !(long_clock_rate&1)) {
			long_length >>= 1;
			long_clock_rate >>= 1;
		}

		if(
			long_length > std::numeric_limits<unsigned int>::max() ||
			long_clock_rate > std::numeric_limits<unsigned int>::max()
		) {
			uint64_t common_divisor = std::gcd(long_length, long_clock_rate);
			long_length /= common_divisor;
			long_clock_rate /= common_divisor;

			// Okay, in desperation accept a loss of accuracy.
			while(
					(
						long_length > std::numeric_limits<unsigned int>::max() ||
						long_clock_rate > std::numeric_limits<unsigned int>::max()
					) &&
					(long_clock_rate > 1)) {
				long_length >>= 1;
				long_clock_rate >>= 1;
			}
		}

		if(
			long_length <= std::numeric_limits<unsigned int>::max() &&
			long_clock_rate <= std::numeric_limits<unsigned int>::max()
		) {
			length = unsigned(long_length);
			clock_rate = unsigned(long_clock_rate);
		} else {
			length = std::numeric_limits<unsigned int>::max();
			clock_rate = 1u;
		}
	}

	void install_float(const float value) {
		// Grab the float's native mantissa and exponent.
		int exponent;
		const float mantissa = frexpf(value, &exponent);

		// Turn the mantissa into an int, and adjust the exponent
		// appropriately.
		const uint64_t loaded_mantissa = uint64_t(ldexpf(mantissa, 24));
		const auto relative_exponent = exponent - 24;

		// If the mantissa is negative and its absolute value fits within a 64-bit integer,
		// just load up.
		if(relative_exponent <= 0 && relative_exponent > -64) {
			install_result(loaded_mantissa, uint64_t(1) << -relative_exponent);
			return;
		}

		// If the exponent is positive but doesn't cause loaded_mantissa to overflow,
		// install with the natural encoding.
		if(relative_exponent > 0 && relative_exponent < (64 - 24)) {
			install_result(loaded_mantissa << relative_exponent, 1);
			return;
		}

		// Otherwise, if this number is too large to store, store the maximum value.
		if(relative_exponent > 0) {
			install_result(std::numeric_limits<uint64_t>::max(), 1);
			return;
		}

		// If the number is too small to store accurately, store 0.
		if(relative_exponent < 0) {
			install_result(0, 1);
			return;
		}
	}
};

}
