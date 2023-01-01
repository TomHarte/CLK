//
//  ClockConverter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef ClockConverter_hpp
#define ClockConverter_hpp

#include "../9918.hpp"

namespace TI {
namespace TMS {

template <Personality personality> class ClockConverter {
	public:
		/*!
			Converts a number of **half-cycles** to an internal number
			of **cycles**.
		*/
		int to_internal(int source) {
			// Default behaviour is top apply a multiplication by 3/4.
			const int result = source * 3 + cycles_error_;
			cycles_error_ = result & 3;
			return result >> 2;
		}

		/*!
			Provides the number of external cycles that will need to pass in order to advance
			_at least_ @c internal_cycles into the future.
		*/
		HalfCycles half_cycles_before_internal_cycles(int internal_cycles) const {
			return HalfCycles(
				((internal_cycles << 2) + (2 - cycles_error_)) / 3
			);
		}

		/*
			Converts a position in internal cycles to its corresponding position
			on the access-window clock.
		*/
		static constexpr int to_access_clock(int source) {
			return source >> 1;
		}

		/// The number of internal cycles in a single line.
		constexpr static int CyclesPerLine = 342;

		/// Indicates the number of access-window cycles in a single line.
		constexpr static int AccessWindowCyclesPerLine = 171;

	private:
		// This implementation of this chip officially accepts a 3.58Mhz clock, but runs
		// internally at 5.37Mhz. The following two help to maintain a lossless conversion
		// from the one to the other.
		int cycles_error_ = 0;
};

}
}

#endif /* ClockConverter_hpp */
