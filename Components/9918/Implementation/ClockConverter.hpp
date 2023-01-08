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

// Timing constants.
template <Personality, typename Enable = void> struct Timing {};

/// Provides default timing measurements that duplicate the layout of a TMS9928's line,
/// scaled to the clock rate specified.
template <int _CyclesPerLine> struct StandardTiming {
	/// The total number of internal cycles per line of output.
	constexpr static int CyclesPerLine = _CyclesPerLine;

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

template <Personality personality>
struct Timing<personality, std::enable_if_t<is_classic_vdp(personality)>>: public StandardTiming<342> {
};

template <Personality personality>
struct Timing<personality, std::enable_if_t<is_yamaha_vdp(personality)>>: public StandardTiming<1368> {
};

template <>
struct Timing<Personality::MDVDP>: public StandardTiming<3420> {
	// Implementation note: descending from StandardTiming works as long as the numbers computed
	// end up being a multiple of 2.5. In practice they're all multiples of 10, so that's guaranteed.
	// Coupled logic is as per to_crt_clock below.
};

constexpr int TMSAccessWindowsPerLine = 171;

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
					return HalfCycles(internal_cycles / 3);

				case Personality::MDVDP:
					return HalfCycles(
						((internal_cycles << 1) + (1 - cycles_error_)) / 7
					);
			}
		}

		/*!
			Converts a position in internal cycles to its corresponding position
			on the TMS memory-access clock, i.e. scales down to 171 clocks
			per line
		*/
		static constexpr int to_tms_access_clock(int source) {
			switch(personality) {
				default:
				return source >> 1;

				case Personality::V9938:
				case Personality::V9958:
				return source >> 3;

				case Personality::MDVDP:
				return source / 20;
			}
		}

		/*!
			Convers a position in internal cycles to its corresponding position
			on the CRT's output clock, which [TODO] is clocked so that
			1368 cycles is 228 NTSC colour cycles.
		*/
		static constexpr int to_crt_clock(int source) {
			switch(personality) {
				default:
				return source * 4;

				case Personality::V9938:
				case Personality::V9958:
				return source;

				case Personality::MDVDP:
				return (source * 2) / 5;
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
