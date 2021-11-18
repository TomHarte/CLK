//
//  ClockReceiver.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef ClockReceiver_hpp
#define ClockReceiver_hpp

#include "ForceInline.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

/*
	Informal pattern for all classes that run from a clock cycle:

		Each will implement either or both of run_for(Cycles) and run_for(HalfCycles), as
		is appropriate.

		Callers that are accumulating HalfCycles but want to talk to receivers that implement
		only run_for(Cycles) can use HalfCycle.flush_cycles if they have appropriate storage, or
		can wrap the receiver in HalfClockReceiver in order automatically to bind half-cycle
		storage to it.

	Alignment rule:

		run_for(Cycles) may be called only after an even number of half cycles. E.g. the following
		sequence will have undefined results:

			run_for(HalfCycles(1))
			run_for(Cycles(1))

		An easy way to ensure this as a caller is to pick only one of run_for(Cycles) and
		run_for(HalfCycles) to use.

	Reasoning:

		Users of this template may with to implement run_for(Cycles) and run_for(HalfCycles)
		where there is a need to implement at half-cycle precision but a faster execution
		path can be offered for full-cycle precision. Those users are permitted to assume
		phase in run_for(Cycles) and should do so to be compatible with callers that use
		only run_for(Cycles).

	Corollary:

		Starting from nothing, the first run_for(HalfCycles(1)) will do the **first** half
		of a full cycle. The second will do the second half. Etc.

*/

/*!
	Provides a class that wraps a plain int, providing most of the basic arithmetic and
	Boolean operators, but forcing callers and receivers to be explicit as to usage.
*/
template <class T> class WrappedInt {
	public:
		using IntType = int64_t;

		forceinline constexpr WrappedInt(IntType l) noexcept : length_(l) {}
		forceinline constexpr WrappedInt() noexcept : length_(0) {}

		forceinline T &operator =(const T &rhs) {
			length_ = rhs.length_;
			return *this;
		}

		forceinline T &operator +=(const T &rhs) {
			length_ += rhs.length_;
			return *static_cast<T *>(this);
		}

		forceinline T &operator -=(const T &rhs) {
			length_ -= rhs.length_;
			return *static_cast<T *>(this);
		}

		forceinline T &operator ++() {
			++ length_;
			return *static_cast<T *>(this);
		}

		forceinline T &operator ++(int) {
			length_ ++;
			return *static_cast<T *>(this);
		}

		forceinline T &operator --() {
			-- length_;
			return *static_cast<T *>(this);
		}

		forceinline T &operator --(int) {
			length_ --;
			return *static_cast<T *>(this);
		}

		forceinline T &operator *=(const T &rhs) {
			length_ *= rhs.length_;
			return *static_cast<T *>(this);
		}

		forceinline T &operator /=(const T &rhs) {
			length_ /= rhs.length_;
			return *static_cast<T *>(this);
		}

		forceinline T &operator %=(const T &rhs) {
			length_ %= rhs.length_;
			return *static_cast<T *>(this);
		}

		forceinline T &operator &=(const T &rhs) {
			length_ &= rhs.length_;
			return *static_cast<T *>(this);
		}

		forceinline constexpr T operator +(const T &rhs) const			{	return T(length_ + rhs.length_);	}
		forceinline constexpr T operator -(const T &rhs) const			{	return T(length_ - rhs.length_);	}

		forceinline constexpr T operator *(const T &rhs) const			{	return T(length_ * rhs.length_);	}
		forceinline constexpr T operator /(const T &rhs) const			{	return T(length_ / rhs.length_);	}

		forceinline constexpr T operator %(const T &rhs) const			{	return T(length_ % rhs.length_);	}
		forceinline constexpr T operator &(const T &rhs) const			{	return T(length_ & rhs.length_);	}

		forceinline constexpr T operator -() const						{	return T(- length_);				}

		forceinline constexpr bool operator <(const T &rhs) const		{	return length_ < rhs.length_;		}
		forceinline constexpr bool operator >(const T &rhs) const		{	return length_ > rhs.length_;		}
		forceinline constexpr bool operator <=(const T &rhs) const		{	return length_ <= rhs.length_;		}
		forceinline constexpr bool operator >=(const T &rhs) const		{	return length_ >= rhs.length_;		}
		forceinline constexpr bool operator ==(const T &rhs) const		{	return length_ == rhs.length_;		}
		forceinline constexpr bool operator !=(const T &rhs) const		{	return length_ != rhs.length_;		}

		forceinline constexpr bool operator !() const					{	return !length_;					}
		// bool operator () is not supported because it offers an implicit cast to int, which is prone silently to permit misuse

		/// @returns The underlying int, converted to an integral type of your choosing, clamped to that int's range.
		template<typename Type = IntType> forceinline constexpr Type as() const {
			const auto clamped = std::clamp(length_, IntType(std::numeric_limits<Type>::min()), IntType(std::numeric_limits<Type>::max()));
			return Type(clamped);
		}

		/// @returns The underlying int, in its native form.
		forceinline constexpr IntType as_integral() const { return length_; }

		/*!
			Severs from @c this the effect of dividing by @c divisor; @c this will end up with
			the value of @c this modulo @c divisor and @c divided by @c divisor is returned.
		*/
		template <typename Result = T> forceinline Result divide(const T &divisor) {
			Result r;
			static_cast<T *>(this)->fill(r, divisor);
			return r;
		}

		/*!
			Flushes the value in @c this. The current value is returned, and the internal value
			is reset to zero.
		*/
		template <typename Result> Result flush() {
			// Jiggery pokery here; switching to function overloading avoids
			// the namespace-level requirement for template specialisation.
			Result r;
			static_cast<T *>(this)->fill(r);
			return r;
		}

		// operator int() is deliberately not provided, to avoid accidental subtitution of
		// classes that use this template.

	protected:
		IntType length_;
};

/// Describes an integer number of whole cycles: pairs of clock signal transitions.
class Cycles: public WrappedInt<Cycles> {
	public:
		forceinline constexpr Cycles(IntType l) noexcept : WrappedInt<Cycles>(l) {}
		forceinline constexpr Cycles() noexcept : WrappedInt<Cycles>() {}
		forceinline static constexpr Cycles max() {
			return Cycles(std::numeric_limits<IntType>::max());
		}

	private:
		friend WrappedInt;
		void fill(Cycles &result) {
			result.length_ = length_;
			length_ = 0;
		}

		void fill(Cycles &result, const Cycles &divisor) {
			result.length_ = length_ / divisor.length_;
			length_ %= divisor.length_;
		}
};

/// Describes an integer number of half cycles: single clock signal transitions.
class HalfCycles: public WrappedInt<HalfCycles> {
	public:
		forceinline constexpr HalfCycles(IntType l) noexcept : WrappedInt<HalfCycles>(l) {}
		forceinline constexpr HalfCycles() noexcept : WrappedInt<HalfCycles>() {}
		forceinline static constexpr HalfCycles max() {
			return HalfCycles(std::numeric_limits<IntType>::max());
		}

		forceinline constexpr HalfCycles(const Cycles &cycles) noexcept : WrappedInt<HalfCycles>(cycles.as_integral() * 2) {}

		/// @returns The number of whole cycles completely covered by this span of half cycles.
		forceinline constexpr Cycles cycles() const {
			return Cycles(length_ >> 1);
		}

		/*!
			Severs from @c this the effect of dividing by @c divisor; @c this will end up with
			the value of @c this modulo @c divisor . @c this divided by @c divisor is returned.
		*/
		forceinline Cycles divide_cycles(const Cycles &divisor) {
			const HalfCycles half_divisor = HalfCycles(divisor);
			const Cycles result(length_ / half_divisor.length_);
			length_ %= half_divisor.length_;
			return result;
		}

		/*!
			Equivalent to @c divide_cycles(Cycles(1)) but faster.
		*/
		forceinline Cycles divide_cycles() {
			const Cycles result(length_ >> 1);
			length_ &= 1;
			return result;
		}

	private:
		friend WrappedInt;
		void fill(Cycles &result) {
			result = Cycles(length_ >> 1);
			length_ &= 1;
		}

		void fill(HalfCycles &result) {
			result.length_ = length_;
			length_ = 0;
		}

		void fill(Cycles &result, const HalfCycles &divisor) {
			result = Cycles(length_ / (divisor.length_ << 1));
			length_ %= (divisor.length_ << 1);
		}

		void fill(HalfCycles &result, const HalfCycles &divisor) {
			result.length_ = length_ / divisor.length_;
			length_ %= divisor.length_;
		}
};

// Create a specialisation of WrappedInt::flush for converting HalfCycles to Cycles
// without losing the fractional part.

/*!
	If a component implements only run_for(Cycles), an owner can wrap it in HalfClockReceiver
	automatically to gain run_for(HalfCycles).
*/
template <class T> class HalfClockReceiver: public T {
	public:
		using T::T;

		forceinline void run_for(const HalfCycles half_cycles) {
			half_cycles_ += half_cycles;
			T::run_for(half_cycles_.flush<Cycles>());
		}

	private:
		HalfCycles half_cycles_;
};

#endif /* ClockReceiver_hpp */
