//
//  Operator.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Operator_hpp
#define Operator_hpp

#include <cstdint>
#include "Tables.hpp"

namespace Yamaha {
namespace OPL {

/*!
	Describes the ephemeral state of an operator.
*/
struct OperatorState {
	public:
		/// @returns The linear output level for the operator with this state..
		int level();

	private:
		LogSign attenuation;
		int raw_phase_ = 0;

		enum class ADSRPhase {
			Attack, Decay, Sustain, Release
		} adsr_phase_ = ADSRPhase::Attack;
		int time_in_phase_ = 0;
		int adsr_attenuation_ = 511;
		bool last_key_on_ = false;

		friend class Operator;
};

/*!
	Describes parts of an operator that are genuinely stored per-operator on the OPLL;
	these can be provided to the Operator in order to have it ignore its local values
	if the host is an OPLL or VRC7.
*/
struct OperatorOverrides {
	int attenuation = 0;
	bool use_sustain_level = false;
};

/*!
	Models an operator.

	In Yamaha FM terms, an operator is a combination of a few things:

		* an oscillator, producing one of a handful of sine-derived waveforms;
		* an ADSR output level envelope; and
		* a bunch of potential adjustments to those two things:
			* optional tremolo and/or vibrato (the rates of which are global);
			* the option to skip 'sustain' in ADSR and go straight to release (since no sustain period is supplied,
				it otherwise runs for as long as the programmer leaves a channel enabled);
			* an attenuation for the output level; and
			* a factor by which to speed up the ADSR envelope as a function of frequency.

	Oscillator period isn't set directly, it's a multiple of the owning channel, in which
	period is set as a combination of f-num and octave.
*/
class Operator {
	public:
		/// Sets this operator's attack rate as the top nibble of @c value, its decay rate as the bottom nibble.
		void set_attack_decay(uint8_t value);

		/// Sets this operator's sustain level as the top nibble of @c value, its release rate as the bottom nibble.
		void set_sustain_release(uint8_t value);

		/// Sets this operator's key scale level as the top two bits of @c value, its total output level as the low six bits.
		void set_scaling_output(uint8_t value);

		/// Sets this operator's waveform using the low two bits of @c value.
		void set_waveform(uint8_t value);

		/// From the top nibble of @c value sets the AM, vibrato, hold/sustain level and keyboard sampling rate flags;
		/// uses the bottom nibble to set the period multiplier.
		void set_am_vibrato_hold_sustain_ksr_multiple(uint8_t value);

		/// Provides one clock tick to the operator, along with the relevant parameters of its channel.
		void update(OperatorState &state, bool key_on, int channel_period, int channel_octave, int phase_offset, OperatorOverrides *overrides = nullptr);

		/// @returns @c true if this channel currently has a non-zero output; @c false otherwise.
		bool is_audible(OperatorState &state, OperatorOverrides *overrides = nullptr);

	private:
		/// If true then an amplitude modulation of "3.7Hz" is applied,
		/// with a depth "determined by the AM-DEPTH of the BD register"?
		bool apply_amplitude_modulation_ = false;

		/// If true then a vibrato of '6.4 Hz' is applied, with a depth
		/// "determined by VOB_DEPTH of the BD register"?
		bool apply_vibrato_ = false;

		/// Selects between an ADSR envelope that holds at the sustain level
		/// for as long as this key is on, releasing afterwards, and one that
		/// simply switches straight to the release rate once the sustain
		/// level is hit, getting back to 0 regardless of an ongoing key-on.
		bool use_sustain_level_ = false;

		/// Indexes a lookup table to determine what multiple of the channel's frequency
		/// this operator is advancing at.
		int frequency_multiple_ = 0;

		/// Sets the current output level of this modulator, as an attenuation.
		int attenuation_ = 0;

		/// Provides a potential faster step through the ADSR envelope. Cf. p12.
		bool key_scaling_rate_ = false;

		/// Selects attenuation that is applied as a function of interval. Cf. p14.
		int level_key_scaling_ = 0;

		/// Sets the ADSR rates. These all provide the top four bits of a six-bit number;
		/// the bottom two bits... are 'RL'?
		int attack_rate_ = 0;
		int decay_rate_ = 0;
		int sustain_level_ = 0;
		int release_rate_ = 0;

		/// Selects the generated waveform.
		enum class Waveform {
			Sine, HalfSine, AbsSine, PulseSine
		} waveform_ = Waveform::Sine;
};

}
}

#endif /* Operator_hpp */
