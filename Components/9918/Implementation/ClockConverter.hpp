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
#include "PersonalityTraits.hpp"

namespace TI {
namespace TMS {

enum class Clock {
	Internal,
	TMSPixel,
	TMSMemoryWindow,
	CRT
};

template <Personality personality, Clock clk> constexpr int clock_rate() {
	static_assert(
		is_classic_vdp(personality) ||
		is_yamaha_vdp(personality) ||
		(personality == Personality::MDVDP)
	);

	switch(clk) {
		case Clock::TMSPixel:			return 342;
		case Clock::TMSMemoryWindow:	return 171;
		case Clock::CRT:				return 1368;
		case Clock::Internal:
			if constexpr (is_classic_vdp(personality)) {
				return 342;
			} else if constexpr (is_yamaha_vdp(personality)) {
				return 1368;
			} else if constexpr (personality == Personality::MDVDP) {
				return 3420;
			}
	}
}

template <Personality personality, Clock clock> constexpr int to_internal(int length) {
	return length * clock_rate<personality, Clock::Internal>() / clock_rate<personality, clock>();
}

template <Personality personality, Clock clock> constexpr int from_internal(int length) {
	return length * clock_rate<personality, clock>() / clock_rate<personality, Clock::Internal>();
}

/// Provides default timing measurements that duplicate the layout of a TMS9928's line,
/// scaled to the clock rate specified.
template <Personality personality> struct StandardTiming {
	/// The total number of internal cycles per line of output.
	constexpr static int CyclesPerLine = clock_rate<personality, Clock::Internal>();

	/// The number of internal cycles that must elapse between a request to read or write and
	/// it becoming a candidate for action.
	constexpr static int VRAMAccessDelay = 6;

	/// The first internal cycle at which pixels will be output in any mode other than text.
	/// Pixels implicitly run from here to the end of the line.
	constexpr static int FirstPixelCycle = 86 * CyclesPerLine / 342;

	/// The first internal cycle at which pixels will be output text mode.
	constexpr static int FirstTextCycle = 94 * CyclesPerLine / 342;

	/// The final internal cycle at which pixels will be output text mode.
	constexpr static int LastTextCycle = 334 * CyclesPerLine / 342;

	// For the below, the fixed portion of line layout is:
	//
	//	[0, EndOfRightBorder):					right border colour
	//	[EndOfRightBorder, StartOfSync):		blank
	//	[StartOfSync, EndOfSync):				sync
	//	[EndOfSync, StartOfColourBurst):		blank
	//	[StartOfColourBurst, EndOfColourBurst):	the colour burst
	//	[EndOfColourBurst, StartOfLeftBorder):	blank
	//
	// The region from StartOfLeftBorder until the end is then filled with
	// some combination of pixels and more border, depending on the vertical
	// position of this line and the current screen mode.
	constexpr static int EndOfRightBorder	= 15 * CyclesPerLine / 342;
	constexpr static int StartOfSync		= 23 * CyclesPerLine / 342;
	constexpr static int EndOfSync			= 49 * CyclesPerLine / 342;
	constexpr static int StartOfColourBurst	= 51 * CyclesPerLine / 342;
	constexpr static int EndOfColourBurst	= 65 * CyclesPerLine / 342;
	constexpr static int StartOfLeftBorder	= 73 * CyclesPerLine / 342;
};

/// Provides concrete, specific timing for the nominated personality.
template <Personality personality> struct Timing: public StandardTiming<personality> {};

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

	Or, put another way, for both a TMS9918 and Master System:

	* 228 external cycles;
	* is 342 internal cycles;
	* which exactly covers 228 NTSC colour clocks; and
	* contains 171 memory access windows.

	Both the Yamaha extensions and the Mega Drive VDP are a bit smarter about
	paged mode memory accesses, obviating any advantage to treating (3) as a
	separate clock.
*/
template <Personality personality> class ClockConverter {
	public:
		/*!
			Given that another @c source external **half-cycles** has occurred,
			indicates how many complete internal **cycles** have additionally elapsed
			since the last call to @c to_internal.

			E.g. for the TMS, @c source will count 456 ticks per line, and the internal clock
			runs at 342 ticks per line, so the proper conversion is to multiply by 3/4.
		*/
		int to_internal(int source) {
			switch(personality) {
				// Default behaviour is to apply a multiplication by 3/4;
				// this is correct for the TMS and Sega VDPs other than the Mega Drive.
				default: {
					const int result = source * 3 + cycles_error_;
					cycles_error_ = result & 3;
					return result >> 2;
				}

				// The two Yamaha chips have an internal clock that is four times
				// as fast as the TMS, therefore a stateless translation is possible.
				case Personality::V9938:
				case Personality::V9958:
				return source * 3;

				// The Mega Drive runs at 3420 master clocks per line, which is then
				// divided by 4 or 5 depending on other state. That's 7 times the
				// rate provided to the CPU; given that the input is in half-cycles
				// the proper multiplier is therefore 3.5.
				case Personality::MDVDP: {
					const int result = source * 7 + cycles_error_;
					cycles_error_ = result & 1;
					return result >> 1;
				}
			}
		}

		/*!
			Provides the number of complete external cycles that lie between now and
			@c internal_cycles into the future. Any trailing fractional external cycle
			is discarded.
		*/
		HalfCycles half_cycles_before_internal_cycles(int internal_cycles) const {
			// Logic here correlates with multipliers as per @c to_internal.
			switch(personality) {
				default:
					return HalfCycles(
						((internal_cycles << 2) + (2 - cycles_error_)) / 3
					);

				case Personality::V9938:
				case Personality::V9958:
					return HalfCycles((internal_cycles + 2) / 3);

				case Personality::MDVDP:
					return HalfCycles(
						((internal_cycles << 1) + (1 - cycles_error_)) / 7
					);
			}
		}

	private:
		// Holds current residue in conversion from the external to
		// internal clock.
		int cycles_error_ = 0;
};

}
}

#endif /* ClockConverter_hpp */
