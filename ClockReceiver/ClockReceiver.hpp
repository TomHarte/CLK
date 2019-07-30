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
		forceinline constexpr WrappedInt(int l) noexcept : length_(l) {}
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

		forceinline constexpr int as_int() const { return length_; }

		/*!
			Severs from @c this the effect of dividing by @c divisor; @c this will end up with
			the value of @c this modulo @c divisor and @c divided by @c divisor is returned.
		*/
		forceinline T divide(const T &divisor) {
			T result(length_ / divisor.length_);
			length_ %= divisor.length_;
			return result;
		}

		/*!
			Flushes the value in @c this. The current value is returned, and the internal value
			is reset to zero.
		*/
		template <typename Result> Result flush() {
			// Jiggery pokery here; switching to function overloading avoids
			// the namespace-level requirement for template specialisation.
			Result r;
			static_cast<T *>(this)->fill_flush(r);
			return r;
		}

		// operator int() is deliberately not provided, to avoid accidental subtitution of
		// classes that use this template.

	protected:
		int length_;
};

/// Describes an integer number of whole cycles: pairs of clock signal transitions.
class Cycles: public WrappedInt<Cycles> {
	public:
		forceinline constexpr Cycles(int l) noexcept : WrappedInt<Cycles>(l) {}
		forceinline constexpr Cycles() noexcept : WrappedInt<Cycles>() {}
		forceinline constexpr Cycles(const Cycles &cycles) noexcept : WrappedInt<Cycles>(cycles.length_) {}

	private:
		friend WrappedInt;
		void fill_flush(Cycles &result) {
			result.length_ = length_;
			length_ = 0;
		}
};

/// Describes an integer number of half cycles: single clock signal transitions.
class HalfCycles: public WrappedInt<HalfCycles> {
	public:
		forceinline constexpr HalfCycles(int l) noexcept : WrappedInt<HalfCycles>(l) {}
		forceinline constexpr HalfCycles() noexcept : WrappedInt<HalfCycles>() {}

		forceinline constexpr HalfCycles(const Cycles &cycles) noexcept : WrappedInt<HalfCycles>(cycles.as_int() * 2) {}
		forceinline constexpr HalfCycles(const HalfCycles &half_cycles) noexcept : WrappedInt<HalfCycles>(half_cycles.length_) {}

		/// @returns The number of whole cycles completely covered by this span of half cycles.
		forceinline constexpr Cycles cycles() const {
			return Cycles(length_ >> 1);
		}

		/*!
			Severs from @c this the effect of dividing by @c divisor; @c this will end up with
			the value of @c this modulo @c divisor and @c divided by @c divisor is returned.
		*/
		forceinline Cycles divide_cycles(const Cycles &divisor) {
			const HalfCycles half_divisor = HalfCycles(divisor);
			const Cycles result(length_ / half_divisor.length_);
			length_ %= half_divisor.length_;
			return result;
		}

	private:
		friend WrappedInt;
		void fill_flush(Cycles &result) {
			result = Cycles(length_ >> 1);
			length_ &= 1;
		}

		void fill_flush(HalfCycles &result) {
			result.length_ = length_;
			length_ = 0;
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
