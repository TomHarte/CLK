//
//  ClockReceiver.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ClockReceiver_hpp
#define ClockReceiver_hpp

template <class T> class WrappedInt {
	public:
		WrappedInt(int l) : length_(l) {}
		WrappedInt() : length_(0) {}

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

		inline T &operator %=(const T &rhs) {
			length_ %= rhs.length_;
			return *static_cast<T *>(this);
		}

		inline T operator +(const T &rhs) const		{	return T(length_ + rhs.length_);	}
		inline T operator -(const T &rhs) const		{	return T(length_ - rhs.length_);	}

		inline bool operator <(const T &rhs) const		{	return length_ < rhs.length_;		}
		inline bool operator >(const T &rhs) const		{	return length_ > rhs.length_;		}
		inline bool operator <=(const T &rhs) const	{	return length_ <= rhs.length_;		}
		inline bool operator >=(const T &rhs) const	{	return length_ >= rhs.length_;		}
		inline bool operator ==(const T &rhs) const	{	return length_ == rhs.length_;		}
		inline bool operator !=(const T &rhs) const	{	return length_ != rhs.length_;		}

		inline bool operator !() const					{	return !length_;					}

		inline int as_int() const { return length_; }

		// operator int() is deliberately not provided, to avoid accidental subtitution of
		// classes that use this template.

	protected:
		int length_;
};

/*! Describes an integer number of whole cycles — pairs of clock signal transitions. */
class Cycles: public WrappedInt<Cycles> {
	public:
		Cycles(int l) : WrappedInt<Cycles>(l) {}
		Cycles() : WrappedInt<Cycles>() {}
		Cycles(const Cycles &cycles) : WrappedInt<Cycles>(cycles.length_) {}
};

/*! Describes an integer number of half cycles — single clock signal transitions. */
class HalfCycles: public WrappedInt<HalfCycles> {
	public:
		HalfCycles(int l) : WrappedInt<HalfCycles>(l) {}
		HalfCycles() : WrappedInt<HalfCycles>() {}

		HalfCycles(const Cycles &cycles) : WrappedInt<HalfCycles>(cycles.as_int() << 1) {}
		HalfCycles(const HalfCycles &half_cycles) : WrappedInt<HalfCycles>(half_cycles.length_) {}
};

/*!
	ClockReceiver is a template for components that receove a clock, measured either
	in cycles or in half cycles. They are expected to implement either of the run_for
	methods; buying into the template means that the other run_for will automatically
	map appropriately to the implemented one, so callers may use either.
*/
template <class T> class ClockReceiver {
	public:
		void run_for(const Cycles &cycles) {
			static_cast<T *>(this)->run_for(HalfCycles(cycles));
		}

		void run_for(const HalfCycles &half_cycles) {
			int cycles = half_cycles.as_int() + half_cycle_carry;
			half_cycle_carry = cycles & 1;
			run_for(Cycles(cycles >> 1));
		}

	private:
		int half_cycle_carry;
};

#endif /* ClockReceiver_hpp */
