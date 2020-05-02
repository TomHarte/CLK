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
*/
template <int precision> class EnvelopeGenerator {
	public:
		/*!
			Advances the envelope generator a single step, given the current state of the low-frequency oscillator, @c oscillator.
		*/
		void update(const LowFrequencyOscillator &oscillator);

		/*!
			@returns The current attenuation from this envelope generator.
		*/
		int attenuation() const;

		/*!
			Enables or disables damping on this envelope generator. If damping is enabled then this envelope generator will
			use the damping phase when necessary (i.e. when transitioning to key on if attenuation is not already at maximum)
			and in any case will call @c will_attack before transitioning from any other state to attack.

			@param will_attack Supply a will_attack callback to enable damping mode; supply nullopt to disable damping mode.
		*/
		void set_should_damp(const std::optional<std::function<void(void)>> &will_attack);

		/*!
			Sets the current state of the key-on input.
		*/
		void set_key_on(bool);

		/*!
			Sets the attack rate, which should be in the range 0–15.
		*/
		void set_attack_rate(int);

		/*!
			Sets the decay rate, which should be in the range 0–15.
		*/
		void set_decay_rate(int);

		/*!
			Sets the release rate, which should be in the range 0–15.
		*/
		void set_release_rate(int);

		/*!
			Sets the sustain level, which should be in the range 0–15.
		*/
		void set_sustain_level(int);

		/*!
			Enables or disables use of the sustain level. If this is disabled, the envelope proceeds
			directly from decay to release.
		*/
		void set_use_sustain_level(bool);

		/*!
			Enables or disables key-rate scaling.
		*/
		void set_key_rate_scaling_enabled(bool enabled);

		/*!
			Enables or disables application of the low-frequency oscillator's tremolo.
		*/
		void set_tremolo_enabled(bool enabled);

		/*!
			Sets the current period associated with the channel that owns this envelope generator;
			this is used to select a key scaling rate if key-rate scaling is enabled.
		*/
		void set_period(int period, int octave);

	private:
		enum class ADSRPhase {
			Attack, Decay, Sustain, Release, Damp
		} adsr_phase_ = ADSRPhase::Attack;
		int adsr_attenuation_ = 511;

		bool key_on_ = false;
		std::optional<std::function<void(void)>> will_attack_;
};

}
}

#endif /* EnvelopeGenerator_h */
