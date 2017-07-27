//
//  ClockReceiver.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ClockReceiver_hpp
#define ClockReceiver_hpp

#include <cstdio>

/*!
	Provides a class that wraps a plain int, providing most of the basic arithmetic and
	Boolean operators, but forcing callers and receivers to be explicit as to usage.
*/
template <class T> class WrappedInt {
	public:
		inline WrappedInt(int l) : length_(l) {}
		inline WrappedInt() : length_(0) {}

		inline T &operator =(const T &rhs) {
			length_ = rhs.length_;
			return *this;
		}

		inline T &operator +=(const T &rhs) {
			length_ += rhs.length_;
			return *static_cast<T *>(this);
		}

		inline T &operator -=(const T &rhs) {
			length_ -= rhs.length_;
			return *static_cast<T *>(this);
		}

		inline T &operator ++() {
			++ length_;
			return *static_cast<T *>(this);
		}

		inline T &operator ++(int) {
			length_ ++;
			return *static_cast<T *>(this);
		}

		inline T &operator --() {
			-- length_;
			return *static_cast<T *>(this);
		}

		inline T &operator --(int) {
			length_ --;
			return *static_cast<T *>(this);
		}

		inline T &operator %=(const T &rhs) {
			length_ %= rhs.length_;
			return *static_cast<T *>(this);
		}

		inline T operator +(const T &rhs) const			{	return T(length_ + rhs.length_);	}
		inline T operator -(const T &rhs) const			{	return T(length_ - rhs.length_);	}

		inline bool operator <(const T &rhs) const		{	return length_ < rhs.length_;		}
		inline bool operator >(const T &rhs) const		{	return length_ > rhs.length_;		}
		inline bool operator <=(const T &rhs) const		{	return length_ <= rhs.length_;		}
		inline bool operator >=(const T &rhs) const		{	return length_ >= rhs.length_;		}
		inline bool operator ==(const T &rhs) const		{	return length_ == rhs.length_;		}
		inline bool operator !=(const T &rhs) const		{	return length_ != rhs.length_;		}

		inline bool operator !() const					{	return !length_;					}
		inline operator bool() const					{	return !!length_;					}

		inline int as_int() const { return length_; }

		/*!
			Severs from @c this the effect of dividing by @c divisor — @c this will end up with
			the value of @c this modulo @c divisor and @c divided by @c divisor is returned.
		*/
		inline T divide(const T &divisor) {
			T result(length_ / divisor.length_);
			length_ %= divisor.length_;
			return result;
		}

		/*!
			Flushes the value in @c this. The current value is returned, and the internal value
			is reset to zero.
		*/
		inline T flush() {
			T result(length_);
			length_ = 0;
			return result;
		}

		// operator int() is deliberately not provided, to avoid accidental subtitution of
		// classes that use this template.

	protected:
		int length_;
};

/// Describes an integer number of whole cycles — pairs of clock signal transitions.
class Cycles: public WrappedInt<Cycles> {
	public:
		inline Cycles(int l) : WrappedInt<Cycles>(l) {}
		inline Cycles() : WrappedInt<Cycles>() {}
		inline Cycles(const Cycles &cycles) : WrappedInt<Cycles>(cycles.length_) {}
};

/// Describes an integer number of half cycles — single clock signal transitions.
class HalfCycles: public WrappedInt<HalfCycles> {
	public:
		inline HalfCycles(int l) : WrappedInt<HalfCycles>(l) {}
		inline HalfCycles() : WrappedInt<HalfCycles>() {}

		inline HalfCycles(const Cycles &cycles) : WrappedInt<HalfCycles>(cycles.as_int() << 1) {}
		inline HalfCycles(const HalfCycles &half_cycles) : WrappedInt<HalfCycles>(half_cycles.length_) {}
};

/*
	Alignment rule:

		run_for(Cycles) may be called only at the start of a cycle. E.g. the following
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
	If a component implements only run_for(Cycles), an owner can wrap it in HalfClockReceiver
	automatically to gain run_for(HalfCycles).
*/
template <class T> class HalfClockReceiver: public T {
	public:
		using T::T;

		using T::run_for;
		inline void run_for(const HalfCycles &half_cycles) {
			int cycles = half_cycles.as_int() + half_cycle_carry_;
			half_cycle_carry_ = cycles & 1;
			T::run_for(Cycles(cycles >> 1));
		}

	private:
		int half_cycle_carry_ = 0;
};

#endif /* ClockReceiver_hpp */
