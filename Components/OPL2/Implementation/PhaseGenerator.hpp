//
//  PhaseGenerator.h
//  Clock Signal
//
//  Created by Thomas Harte on 30/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef PhaseGenerator_h
#define PhaseGenerator_h

#include <cassert>
#include "LowFrequencyOscillator.hpp"
#include "Tables.hpp"

namespace Yamaha {
namespace OPL {

/*!
	Models an OPL-style phase generator of templated precision; having been told its period ('f-num'), octave ('block') and
	multiple, and whether to apply vibrato, this will then appropriately update and return phase.
*/
template <int precision> class PhaseGenerator {
	public:
		/*!
			Advances the phase generator a single step, given the current state of the low-frequency oscillator, @c oscillator.
		*/
		void update(const LowFrequencyOscillator &oscillator) {
			constexpr int vibrato_shifts[8] = {3, 1, 0, 1, 3, 1, 0, 1};
			constexpr int vibrato_signs[2] = {1, -1};

			// Get just the top three bits of the period_.
			const int top_freq = period_ >> (precision - 3);

			// Cacluaute applicable vibrato as a function of (i) the top three bits of the
			// oscillator period; (ii) the current low-frequency oscillator vibrato output; and
			// (iii) whether vibrato is enabled.
			const int vibrato = (top_freq >> vibrato_shifts[oscillator.vibrato]) * vibrato_signs[oscillator.vibrato >> 2] * enable_vibrato_;

			// Apply phase update with vibrato from the low-frequency oscillator.
			phase_ += multiple_ * (period_ + vibrato) << octave_;
		}


		/*!
			@returns Current phase; real hardware provides only the low ten bits of this result.
		*/
		int phase() const {
			// My table if multipliers is multiplied by two, so shift by one more
			// than the stated precision.
			return phase_ >> precision_shift;
		}

		/*!
			@returns Current phase, scaled up by (1 << precision).
		*/
		int scaled_phase() const {
			return phase_ >> 1;
		}

		/*!
			Sets the multiple for this phase generator, in the same terms as an OPL programmer,
			i.e. a 4-bit number that is used as a lookup into the internal multiples table.
		*/
		void set_multiple(int multiple) {
			// This encodes the MUL -> multiple table given on page 12,
			// multiplied by two.
			constexpr int multipliers[] = {
				1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
			};
			assert(multiple < 16);
			multiple_ = multipliers[multiple];
		}

		/*!
			Sets the period of this generator, along with its current octave.

			Yamaha tends to refer to the period as the 'f-number', and used both 'octave' and 'block' for octave.
		*/
		void set_period(int period, int octave) {
			period_ = period;
			octave_ = octave;

			assert(octave_ < 8);
			assert(period_ < (1 << precision));
		}

		/*!
			Enables or disables vibrato.
		*/
		void set_vibrato_enabled(bool enabled) {
			enable_vibrato_ = int(enabled);
		}

		/*!
			Resets the current phase.
		*/
		void reset() {
			phase_ = 0;
		}

	private:
		static constexpr int precision_shift =  1 + precision;

		int phase_ = 0;

		int multiple_ = 0;
		int period_ = 0;
		int octave_ = 0;
		int enable_vibrato_ = 0;
};

}
}

#endif /* PhaseGenerator_h */
