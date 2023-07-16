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
#include "LineLayout.hpp"

namespace TI::TMS {

enum class Clock {
	/// Whatever rate this VDP runs at, with location 0 being "the start" of the line per internal preference.
	Internal,
	/// A 342-cycle/line clock with the same start position as ::Internal.
	TMSPixel,
	/// A 171-cycle/line clock that begins at the memory window which starts straight after ::Internal = 0.
	TMSMemoryWindow,
	/// A fixed 1368-cycle/line clock that is used to count output to the CRT.
	CRT,
};

enum class Origin {
	///
	ModeLatch,

	/// Provides the same clock rate as ::Internal but is relocated so that 0 is the start of horizontal sync — very not coincidentally,
	/// where Grauw puts 0 on his detailed TMS and Yamaha timing diagrams.
	StartOfSync,
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

template <Personality personality, Clock head, Clock... tail> constexpr int to_internal(int length) {
	length = length * clock_rate<personality, Clock::Internal>() / clock_rate<personality, head>();

	if constexpr (!sizeof...(tail)) {
		return length;
	} else {
		return to_internal<personality, tail...>(length);
	}
}

template <Personality personality, Origin origin> constexpr int to_internal(int length) {
	if constexpr (origin == Origin::ModeLatch) {
		return (
			length + LineLayout<personality>::CyclesPerLine - LineLayout<personality>::ModeLatchCycle
		) % LineLayout<personality>::CyclesPerLine;
	}
	return length;
}

/// Statelessly converts @c length to the internal clock for @c personality; applies conversions per the list of clocks in left-to-right order.
template <Personality personality, Origin origin, Clock head, Clock... tail> constexpr int to_internal(int length) {
	length = to_internal<personality, head, tail...>(length);
	return to_internal<personality, origin>(length);
}

template <Personality personality, Clock head, Clock... tail> constexpr int from_internal(int length) {
	length = length * clock_rate<personality, head>() / clock_rate<personality, Clock::Internal>();

	if constexpr (!sizeof...(tail)) {
		return length;
	} else {
		return to_internal<personality, tail...>(length);
	}
}

template <Personality personality, Origin origin> constexpr int from_internal(int length) {
	if constexpr (origin == Origin::ModeLatch) {
		return (
			length + LineLayout<personality>::ModeLatchCycle
		) % LineLayout<personality>::CyclesPerLine;
	}
	return length;
}

/// Statelessly converts @c length to @c clock from the the internal clock used by VDPs of @c personality throwing away any remainder.
template <Personality personality, Origin origin, Clock head, Clock... tail> constexpr int from_internal(int length) {
	length = from_internal<personality, origin>(length);
	return from_internal<personality, head, tail...>(length);
}

/*!
	Provides a [potentially-]stateful conversion between the external and internal clocks.
	Unlike the other clock conversions, this may be non-integral, requiring that
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

}

#endif /* ClockConverter_hpp */
