//
//  Channel.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Channel_hpp
#define Channel_hpp

#include "LowFrequencyOscillator.hpp"
#include "Operator.hpp"

namespace Yamaha {
namespace OPL {

/*!
	Models an L-type two-operator channel.

	Assuming FM synthesis is enabled, the channel modulates the output of the carrier with that of the modulator.

	TODO: make this a template on period counter size in bits?
*/
class Channel {
	public:
		/// Sets the low 8 bits of frequency control.
		void set_frequency_low(uint8_t value);

		/// Sets the high two bits of a 10-bit frequency control, along with this channel's
		/// block/octave, and key on or off.
		void set_10bit_frequency_octave_key_on(uint8_t value);

		/// Sets the high two bits of a 9-bit frequency control, along with this channel's
		/// block/octave, and key on or off.
		void set_9bit_frequency_octave_key_on(uint8_t value);

		/// Sets the amount of feedback provided to the first operator (i.e. the modulator)
		/// associated with this channel, and whether FM synthesis is in use.
		void set_feedback_mode(uint8_t value);

		/// Updates one of this channel's operators.
		void update(bool modulator, const LowFrequencyOscillator &oscillator, Operator &op, bool force_key_on = false, OperatorOverrides *overrides = nullptr);

		/// Gets regular 'melodic' output for this channel, i.e. the output you'd expect from all channels when not in rhythm mode.
		int melodic_output(const Operator &modulator, const Operator &carrier, const OperatorOverrides *overrides = nullptr) const;

		/// Generates tom tom output using this channel's modulator.
		int tom_tom_output(const Operator &modulator, const OperatorOverrides *overrides = nullptr) const;

		/// Generates snare output, using this channel's carrier.
		int snare_output(const Operator &carrier, const OperatorOverrides *overrides = nullptr) const;

		/// Generates cymbal output, using this channel's modulator and @c channel8 's carrier.
		int cymbal_output(const Operator &modulator, const Operator &carrier, const Channel &channel8, const OperatorOverrides *overrides = nullptr) const;

		/// Generates cymbal output, using this channel's modulator and @c channel8 's carrier.
		int high_hat_output(const Operator &modulator, const Operator &carrier, const Channel &channel8, const  OperatorOverrides *overrides = nullptr) const;

		/// @returns @c true if this channel is currently producing any audio; @c false otherwise;
		bool is_audible(Operator *carrier, OperatorOverrides *carrier_overrides = nullptr);

	private:
		/// 'F-Num' in the spec; this plus the current octave determines channel frequency.
		int period_ = 0;

		/// Linked with the frequency, determines the channel frequency.
		int octave_ = 0;

		/// Sets sets this channel on or off, as an input to the ADSR envelope,
		bool key_on_ = false;

		/// Sets the degree of feedback applied to the modulator.
		int feedback_strength_ = 0;

		/// Selects between FM synthesis, using the modulator to modulate the carrier, or simple mixing of the two
		/// underlying operators as completely disjoint entities.
		bool use_fm_synthesis_ = true;

		/// Used internally to make both the 10-bit OPL2 frequency selection and 9-bit OPLL/VRC7 frequency
		/// selections look the same when passed to the operators.
		int frequency_shift_ = 0;

		// Operator state is stored distinctly from Operators because
		// carrier/modulator may not be unique per channel —
		// on the OPLL there's an extra level of indirection.
		OperatorState states_[2];
};

}
}

#endif /* Channel_hpp */
