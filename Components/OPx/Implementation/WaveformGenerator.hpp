//
//  WaveformGenerator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef WaveformGenerator_h
#define WaveformGenerator_h

#include "Tables.hpp"
#include "LowFrequencyOscillator.hpp"

namespace Yamaha {
namespace OPL {

enum class Waveform {
	Sine, HalfSine, AbsSine, PulseSine
};

template <int phase_precision> class WaveformGenerator {
	public:
		/*!
			@returns The output of waveform @c form at [integral] phase @c phase.
		*/
		static constexpr LogSign wave(Waveform form, int phase) {
			constexpr int waveforms[4][4] = {
				{1023, 1023, 1023, 1023},	// Sine: don't mask in any quadrant.
				{511, 511, 0, 0},			// Half sine: keep the first half intact, lock to 0 in the second half.
				{511, 511, 511, 511},		// AbsSine: endlessly repeat the first half of the sine wave.
				{255, 0, 255, 0},			// PulseSine: act as if the first quadrant is in the first and third; lock the other two to 0.
			};
			return negative_log_sin(phase & waveforms[int(form)][(phase >> 8) & 3]);
		}

		/*!
			@returns The output of waveform @c form at [scaled] phase @c scaled_phase given the modulation input @c modulation.
		*/
		static constexpr LogSign wave(Waveform form, int scaled_phase, LogSign modulation) {
			const int scaled_phase_offset = modulation.level(phase_precision);
			const int phase = (scaled_phase + scaled_phase_offset) >> phase_precision;
			return wave(form, phase);
		}

		/*!
			@returns Snare output, calculated from the current LFSR state as captured in @c oscillator and an operator's phase.
		*/
		static constexpr LogSign snare(const LowFrequencyOscillator &oscillator, int phase) {
			// If noise is 0, output is positive.
			// If noise is 1, output is negative.
			// If (noise ^ sign) is 0, output is 0. Otherwise it is max.
			const int sign = phase & 0x200;
			const int level = ((phase >> 9) & 1) ^ oscillator.lfsr;
			return negative_log_sin(sign + (level << 8));
		}

		/*!
			@returns Cymbal output, calculated from an operator's phase and a modulator's phase.
		*/
		static constexpr LogSign cymbal(int carrier_phase, int modulator_phase) {
			return negative_log_sin(256 + (phase_combination(carrier_phase, modulator_phase) << 9));
		}

		/*!
			@returns High-hat output, calculated from the current LFSR state as captured in @c oscillator, an operator's phase and a modulator's phase.
		*/
		static constexpr LogSign high_hat(const LowFrequencyOscillator &oscillator, int carrier_phase, int modulator_phase) {
			constexpr int angles[] = {0x234, 0xd0, 0x2d0, 0x34};
			return negative_log_sin(angles[
				phase_combination(carrier_phase, modulator_phase) |
				(oscillator.lfsr << 1)
			]);
		}

	private:
		/*!
			@returns The phase bit used for cymbal and high-hat generation, which is a function of two operators' phases.
		*/
		static constexpr int phase_combination(int carrier_phase, int modulator_phase) {
			return (
				((carrier_phase >> 5) ^ (carrier_phase >> 3)) &
				((modulator_phase >> 7) ^ (modulator_phase >> 2)) &
				((carrier_phase >> 5) ^ (modulator_phase >> 3))
			) & 1;
		}
};

}
}

#endif /* WaveformGenerator_h */
