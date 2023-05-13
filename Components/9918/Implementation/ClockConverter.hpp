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

namespace TI::TMS {

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

/*!
	Provides a [potentially-]stateful conversion between the external and internal clocks.
	Unlike the other clock conversions, this one may be non-integral, requiring that
	an error term be tracked.
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
			Provides the number of external cycles that need to begin from now in order to
			get at least @c internal_cycles into the future.
		*/
		HalfCycles half_cycles_before_internal_cycles(int internal_cycles) const {
			// Logic here correlates with multipliers as per @c to_internal.
			switch(personality) {
				default:
					// Relative to the external clock multiplied by 3, it will definitely take this
					// many cycles to complete a further (internal_cycles - 1) after the current one.
					internal_cycles = (internal_cycles - 1) << 2;

					// It will also be necessary to complete the current one.
					internal_cycles += 4 - cycles_error_;

					// Round up to get the first external cycle after
					// the number of internal_cycles has elapsed.
					return HalfCycles((internal_cycles + 2) / 3);

				case Personality::V9938:
				case Personality::V9958:
					return HalfCycles((internal_cycles + 2) / 3);

				case Personality::MDVDP:
					internal_cycles = (internal_cycles - 1) << 1;
					internal_cycles += 2 - cycles_error_;
					return HalfCycles((internal_cycles + 6) / 7);
			}
		}

	private:
		// Holds current residue in conversion from the external to
		// internal clock.
		int cycles_error_ = 0;
};


//
//
//
template <Personality personality, typename Enable = void> struct LineLayout;

//	Line layout is:
//
//	[0, EndOfSync]							sync
//	(EndOfSync, StartOfColourBurst]			blank
//	(StartOfColourBurst, EndOfColourBurst]	colour burst
//	(EndOfColourBurst, EndOfLeftErase]		blank
//	(EndOfLeftErase, EndOfLeftBorder]		border colour
//	(EndOfLeftBorder, EndOfPixels]			pixel content
//	(EndOfPixels, EndOfRightBorder]			border colour
//	[EndOfRightBorder, <end of line>]		blank
//
//	... with minor caveats:
//		* horizontal adjust on the Yamaha VDPs is applied to EndOfLeftBorder and EndOfPixels;
//		* the Sega VDPs may programatically extend the left border; and
//		* text mode on all VDPs adjusts border width.

template <Personality personality> struct LineLayout<personality, std::enable_if_t<is_classic_vdp(personality)>> {
	constexpr static int EndOfSync			= 26;
	constexpr static int StartOfColourBurst	= 29;
	constexpr static int EndOfColourBurst	= 43;
	constexpr static int EndOfLeftErase		= 50;
	constexpr static int EndOfLeftBorder	= 63;
	constexpr static int EndOfPixels		= 319;
	constexpr static int EndOfRightBorder	= 334;

	constexpr static int CyclesPerLine		= 342;

	constexpr static int TextModeEndOfLeftBorder	= 69;
	constexpr static int TextModeEndOfPixels		= 309;

	/// The number of internal cycles that must elapse between a request to read or write and
	/// it becoming a candidate for action.
	constexpr static int VRAMAccessDelay = 6;
};

template <Personality personality> struct LineLayout<personality, std::enable_if_t<is_yamaha_vdp(personality)>> {
	constexpr static int EndOfSync			= 100;
	constexpr static int StartOfColourBurst	= 113;
	constexpr static int EndOfColourBurst	= 167;
	constexpr static int EndOfLeftErase		= 202;
	constexpr static int EndOfLeftBorder	= 258;
	constexpr static int EndOfPixels		= 1282;
	constexpr static int EndOfRightBorder	= 1341;

	constexpr static int CyclesPerLine		= 1368;

	constexpr static int TextModeEndOfLeftBorder	= 294;
	constexpr static int TextModeEndOfPixels		= 1254;

	/// The number of internal cycles that must elapse between a request to read or write and
	/// it becoming a candidate for action.
	constexpr static int VRAMAccessDelay = 16;
};

}

#endif /* ClockConverter_hpp */
