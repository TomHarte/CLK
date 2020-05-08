//
//  EnvelopeGenerator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef EnvelopeGenerator_h
#define EnvelopeGenerator_h

#include <optional>
#include <functional>
#include "LowFrequencyOscillator.hpp"

namespace Yamaha {
namespace OPL {

/*!
	Models an OPL-style envelope generator.

	Damping is optional; if damping is enabled then if there is a transition to key-on while
	attenuation is less than maximum then attenuation will be quickly transitioned to maximum
	before the attack phase can begin.

	in real hardware damping is used by the envelope generators associated with
	carriers, with phases being reset upon the transition from damping to attack.

	This code considers application of tremolo to be a function of the envelope generator;
	this is largely for logical conformity with the phase generator that necessarily has to
	apply vibrato.

	TODO: use envelope_precision.
*/
template <int envelope_precision, int period_precision> class EnvelopeGenerator {
	public:
		/*!
			Advances the envelope generator a single step, given the current state of the low-frequency oscillator, @c oscillator.
		*/
		void update(const LowFrequencyOscillator &oscillator) {
			// Apply tremolo, which is fairly easy.
			tremolo_ = tremolo_enable_ * oscillator.tremolo << 4;

			// Something something something...
			const int key_scaling_rate = key_scale_rate_ >> key_scale_rate_shift_;
			switch(phase_) {
				case Phase::Damp:
					update_decay(oscillator, 12 << 2);
					if(attenuation_ == 511) {
						(*will_attack_)();
						phase_ = Phase::Attack;
					}
				break;

				case Phase::Attack:
					update_attack(oscillator, attack_rate_ + key_scaling_rate);

					// Two possible terminating conditions: (i) the attack rate is 15; (ii) full volume has been reached.
					if((attack_rate_ + key_scaling_rate) > 60 || attenuation_ <= 0) {
						attenuation_ = 0;
						phase_ = Phase::Decay;
					}
				break;

				case Phase::Decay:
					update_decay(oscillator, decay_rate_ + key_scaling_rate);
					if(attenuation_ >= sustain_level_) {
						attenuation_ = sustain_level_;
						phase_ = use_sustain_level_ ? Phase::Sustain : Phase::Release;
					}
				break;

				case Phase::Sustain:
					// Nothing to do.
				break;

				case Phase::Release:
					update_decay(oscillator, release_rate_ + key_scaling_rate);
				break;
			}
		}

		/*!
			@returns The current attenuation from this envelope generator. This is independent of the envelope precision.
		*/
		int attenuation() const {
			return (attenuation_ + tremolo_) << 3;
		}

		/*!
			Enables or disables damping on this envelope generator. If damping is enabled then this envelope generator will
			use the damping phase when necessary (i.e. when transitioning to key on if attenuation is not already at maximum)
			and in any case will call @c will_attack before transitioning from any other state to attack.

			@param will_attack Supply a will_attack callback to enable damping mode; supply nullopt to disable damping mode.
		*/
		void set_should_damp(const std::optional<std::function<void(void)>> &will_attack) {
			will_attack_ = will_attack;
		}

		/*!
			Sets the current state of the key-on input.
		*/
		void set_key_on(bool key_on) {
			// Do nothing if this is not a leading or trailing edge.
			if(key_on == key_on_) return;
			key_on_ = key_on;

			// Always transition to release upon a key off.
			if(!key_on_) {
				phase_ = Phase::Release;
				return;
			}

			// On key on: if this is an envelope generator with damping, and damping is required,
			// schedule that. If damping is not required, announce a pending attack now and
			// transition to attack.
			if(will_attack_) {
				if(attenuation_ != 511) {
					phase_ = Phase::Damp;
					return;
				}

				(*will_attack_)();
			}
			phase_ = Phase::Attack;
		}

		/*!
			Sets the attack rate, which should be in the range 0–15.
		*/
		void set_attack_rate(int rate) {
			attack_rate_ = rate << 2;
		}

		/*!
			Sets the decay rate, which should be in the range 0–15.
		*/
		void set_decay_rate(int rate) {
			decay_rate_ = rate << 2;
		}

		/*!
			Sets the release rate, which should be in the range 0–15.
		*/
		void set_release_rate(int rate) {
			release_rate_ = rate << 2;
		}

		/*!
			Sets the sustain level, which should be in the range 0–15.
		*/
		void set_sustain_level(int level) {
			sustain_level_ = level << 3;
			// TODO: verify the shift level here. Especially re: precision.
		}

		/*!
			Enables or disables use of the sustain level. If this is disabled, the envelope proceeds
			directly from decay to release.
		*/
		void set_use_sustain_level(bool use) {
			use_sustain_level_ = use;
		}

		/*!
			Enables or disables key-rate scaling.
		*/
		void set_key_scaling_rate_enabled(bool enabled) {
			key_scale_rate_shift_ = int(enabled) * 2;
		}

		/*!
			Enables or disables application of the low-frequency oscillator's tremolo.
		*/
		void set_tremolo_enabled(bool enabled) {
			tremolo_enable_ = int(enabled);
		}

		/*!
			Sets the current period associated with the channel that owns this envelope generator;
			this is used to select a key scaling rate if key-rate scaling is enabled.
		*/
		void set_period(int period, int octave) {
			key_scale_rate_ = (octave << 1) | (period >> (period_precision - 1));
		}

	private:
		enum class Phase {
			Attack, Decay, Sustain, Release, Damp
		} phase_ = Phase::Release;
		int attenuation_ = 511, tremolo_ = 0;

		bool key_on_ = false;
		std::optional<std::function<void(void)>> will_attack_;

		int key_scale_rate_ = 0;
		int key_scale_rate_shift_ = 0;

		int tremolo_enable_ = 0;

		int attack_rate_ = 0;
		int decay_rate_ = 0;
		int release_rate_ = 0;
		int sustain_level_ = 0;
		bool use_sustain_level_ = false;

		static constexpr int dithering_patterns[4][8] = {
			{0, 1, 0, 1, 0, 1, 0, 1},
			{0, 1, 0, 1, 1, 1, 0, 1},
			{0, 1, 1, 1, 0, 1, 1, 1},
			{0, 1, 1, 1, 1, 1, 1, 1},
		};

		void update_attack(const LowFrequencyOscillator &oscillator, int rate) {
			// Rules:
			//
			// An attack rate of '13' has 32 samples in the attack phase; a rate of '12' has the same 32 steps, but spread out over 64 samples, etc.
			// An attack rate of '14' uses a divide by four instead of two.
			// 15 is instantaneous.

			if(rate >= 56) {
				attenuation_ -= (attenuation_ >> 2) - 1;
			} else {
				const int sample_length = 1 << (14 - (rate >> 2));	// TODO: don't throw away KSR bits.
				if(!(oscillator.counter & (sample_length - 1))) {
					attenuation_ -= (attenuation_ >> 3) - 1;
				}
			}
		}

		void update_decay(const LowFrequencyOscillator &oscillator, int rate) {
			// Special case: no decay.
			if(rate < 4) {
				return;
			}

			// Work out the number of cycles between each adjustment tick, and stop now
			// if not at the next adjustment boundary.
			const int shift_size = 13 - (std::min(rate, 52) >> 2);
			if(oscillator.counter & ((1 << shift_size) - 1)) {
				return;
			}

			// Apply dithered adjustment and clamp.
			const int rate_shift = 1 + (rate > 59) + (rate > 55);
			attenuation_ += dithering_patterns[rate & 3][(oscillator.counter >> shift_size) & 7] * (4 << rate_shift);
			attenuation_ = std::min(attenuation_, 511);
		}
};

}
}

#endif /* EnvelopeGenerator_h */
