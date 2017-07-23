//
//  ClockReceiver.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ClockReceiver_hpp
#define ClockReceiver_hpp

/*! Describes an integer number of whole cycles — pairs of clock signal transitions. */
class Cycles {
	public:
		Cycles(int l) : length_(l) {}
		operator int() const { return length_; }

	private:
		int length_;
};

/*! Describes an integer number of half cycles — single clock signal transitions. */
class HalfCycles {
	public:
		HalfCycles(int l) : length_(l) {}
		HalfCycles(const Cycles &cycles) : length_(int(cycles) << 1) {}
		operator int() const { return length_; }

	private:
		int length_;
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
			int cycles = int(half_cycles) + half_cycle_carry;
			half_cycle_carry = cycles & 1;
			run_for(Cycles(cycles >> 1));
		}

	private:
		int half_cycle_carry;
};

#endif /* ClockReceiver_hpp */
