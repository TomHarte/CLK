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

/*!
	This implementation of the TMS, etc mediates between three clocks:

	1)	the external clock, which is whatever the rest of the system(s)
		it plugs into run at;

	2)	the internal clock, which is used to time and place syncs, borders,
		pixel regions, etc; and

	3)	a memory acccess clock, which correlates to the number of windows
		available for memory accesses.

	E.g. for both a regular TMS9918 and the Sega Master System, the external
	clock is 3.58Mhz, the internal clock is 5.37Mhz and the memory access
	clock is 2.69Mhz.

	Both the Yamaha extensions and the Mega Drive VDP are a bit smarter about
	paged mode memory accesses, obviating any advantage to treating (3) as a
	separate clock.
*/
template <Personality personality> class ClockConverter {
	public:
		/*!
			Converts a number of extenral **half-cycles** to an internal number
			of **cycles**.
		*/
		int to_internal(int source) {
			// Default behaviour is top apply a multiplication by 3/4.
			const int result = source * 3 + cycles_error_;
			cycles_error_ = result & 3;
			return result >> 2;
		}

		/*!
			Provides the number of complete external cycles that lie between now and
			@c internal_cycles into the future. Any trailing fractional external cycle
			is discarded.
		*/
		HalfCycles half_cycles_before_internal_cycles(int internal_cycles) const {
			return HalfCycles(
				((internal_cycles << 2) + (2 - cycles_error_)) / 3
			);
		}

		/*!
			Converts a position in internal cycles to its corresponding position
			on the memory-access clock.
		*/
		static constexpr int to_access_clock(int source) {
			return source >> 1;
		}

		/// The number of internal cycles in a single line.
		constexpr static int CyclesPerLine = 342;

		/// Indicates the number of access-window cycles in a single line.
		constexpr static int AccessWindowCyclesPerLine = 171;

	private:
		// Holds current residue in conversion from the external to
		// internal clock.
		int cycles_error_ = 0;
};

}
}

#endif /* ClockConverter_hpp */
