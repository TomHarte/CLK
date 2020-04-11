//
//  OPL2.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef OPL2_hpp
#define OPL2_hpp

#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"
#include "../../Numeric/LFSR.hpp"

namespace Yamaha {


namespace OPL {

/*!
	Describes the ephemeral state of an operator.
*/
struct OperatorState {
	public:
		int phase = 0;		// Will be in the range [0, 1023], mapping into a 1024-unit sine curve.
		int volume = 0;

	private:
		int divider_ = 0;
		int raw_phase_ = 0;

		friend class Operator;
};

/*!
	Describes parts of an operator that are genuinely stored per-operator on the OPLL;
	these can be provided to the Operator in order to have it ignore its local values
	if the host is an OPLL or VRC7.
*/
struct OperatorOverrides {
	int output_level = 0;
	bool hold_sustain_level = false;
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

	Oscillator frequency isn't set directly, it's a multiple of the owning channel, in which
	frequency is set as a combination of f-num and octave.
*/
class Operator {
	public:
		/// Sets this operator's attack rate as the top nibble of @c value, its decay rate as the bottom nibble.
		void set_attack_decay(uint8_t value) {
			attack_rate = value >> 4;
			decay_rate = value & 0xf;
		}

		/// Sets this operator's sustain level as the top nibble of @c value, its release rate as the bottom nibble.
		void set_sustain_release(uint8_t value) {
			sustain_level = value >> 4;
			release_rate = value & 0xf;
		}

		/// Sets this operator's key scale level as the top two bits of @c value, its total output level as the low six bits.
		void set_scaling_output(uint8_t value) {
			scaling_level = value >> 6;
			output_level = value & 0x3f;
		}

		/// Sets this operator's waveform using the low two bits of @c value.
		void set_waveform(uint8_t value) {
			waveform = Operator::Waveform(value & 3);
		}

		/// From the top nibble of @c value sets the AM, vibrato, hold/sustain level and keyboard sampling rate flags;
		/// uses the bottom nibble to set the frequency multiplier.
		void set_am_vibrato_hold_sustain_ksr_multiple(uint8_t value) {
			apply_amplitude_modulation = value & 0x80;
			apply_vibrato = value & 0x40;
			hold_sustain_level = value & 0x20;
			keyboard_scaling_rate = value & 0x10;
			frequency_multiple = value & 0xf;
		}

		void update(OperatorState &state, int channel_frequency, int channel_octave, OperatorOverrides *overrides = nullptr) {
			// Per the documentation:
			// F-Num = Music Frequency * 2^(20-Block) / 49716
			//
			// Given that a 256-entry table is used to store a quarter of a sine wave,
			// making 1024 steps per complete wave, add what I've called frequency
			// to an accumulator and move on whenever that exceeds 2^(10 - octave).
			//
			// ... subject to each operator having a frequency multiple.
			//
			// Or: 2^19?

			// This encodes the MUL -> multiple table given on page 12,
			// multiplied by two.
			constexpr int multipliers[] = {
				1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
			};

			// Update the raw phase.
			const int octave_divider = (10 - channel_octave) << 9;
			state.divider_ += multipliers[frequency_multiple] * channel_frequency;
			state.raw_phase_ += state.divider_ / octave_divider;
			state.divider_ %= octave_divider;

			// Hence calculate phase (TODO: by also taking account of vibrato).
			constexpr int waveforms[4][4] = {
				{1023, 1023, 1023, 1023},	// Sine: don't mask in any quadrant.
				{511, 511, 0, 0},			// Half sine: keep the first half in tact, lock to 0 in the second half.
				{511, 511, 511, 511},		// AbsSine: endlessly repeat the first half of the sine wave.
				{255, 0, 255, 0},			// PulseSine: act as if the first quadrant is in the first and third; lock the other two to 0.
			};
			state.phase = state.raw_phase_ & waveforms[int(waveform)][(state.raw_phase_ >> 8) & 3];

			// TODO: calculate output volume properly; apply: ADSR and amplitude modulation (tremolo, I assume?)
			state.volume = output_level;
		}

	private:
		/// If true then an amplitude modulation of "3.7Hz" is applied,
		/// with a depth "determined by the AM-DEPTH of the BD register"?
		bool apply_amplitude_modulation = false;

		/// If true then a vibrato of '6.4 Hz' is applied, with a depth
		/// "determined by VOB_DEPTH of the BD register"?
		bool apply_vibrato = false;

		/// Selects between an ADSR envelope that holds at the sustain level
		/// for as long as this key is on, releasing afterwards, and one that
		/// simply switches straight to the release rate once the sustain
		/// level is hit, getting back to 0 regardless of an ongoing key-on.
		bool hold_sustain_level = false;

		/// Provides a potential faster step through the ADSR envelope. Cf. p12.
		bool keyboard_scaling_rate = false;

		/// Indexes a lookup table to determine what multiple of the channel's frequency
		/// this operator is advancing at.
		int frequency_multiple = 0;

		/// Sets the current output level of this modulator, as an attenuation.
		int output_level = 0;

		/// Selects attenuation that is applied as a function of interval. Cf. p14.
		int scaling_level = 0;

		/// Sets the ADSR rates.
		int attack_rate = 0;
		int decay_rate = 0;
		int sustain_level = 0;
		int release_rate = 0;

		/// Selects the generated waveform.
		enum class Waveform {
			Sine, HalfSine, AbsSine, PulseSine
		} waveform = Waveform::Sine;
};

/*!
	Models an L-type two-operator channel.

	Assuming FM synthesis is enabled, the channel modulates the output of the carrier with that of the modulator.
*/
class Channel {
	public:
		/// Sets the low 8 bits of frequency control.
		void set_frequency_low(uint8_t value) {
			frequency = (frequency &~0xff) | value;
		}

		/// Sets the high two bits of a 10-bit frequency control, along with this channel's
		/// block/octave, and key on or off.
		void set_10bit_frequency_octave_key_on(uint8_t value) {
			frequency = (frequency & 0xff) | ((value & 3) << 8);
			octave = (value >> 2) & 0x7;
			key_on = value & 0x20;;
		}

		/// Sets the high two bits of a 9-bit frequency control, along with this channel's
		/// block/octave, and key on or off.
		void set_9bit_frequency_octave_key_on(uint8_t value) {
			frequency = (frequency & 0xff) | ((value & 1) << 8);
			octave = (value >> 1) & 0x7;
			key_on = value & 0x10;;
		}

		/// Sets the amount of feedback provided to the first operator (i.e. the modulator)
		/// associated with this channel, and whether FM synthesis is in use.
		void set_feedback_mode(uint8_t value) {
			feedback_strength = (value >> 1) & 0x7;
			use_fm_synthesis = value & 1;
		}

		// This should be called at a rate of around 49,716 Hz.
		void update(Operator *carrier, Operator *modulator) {
			modulator->update(modulator_state_, frequency, octave);
			carrier->update(carrier_state_, frequency, octave);
		}

	private:
		/// 'F-Num' in the spec; this plus the current octave determines channel frequency.
		int frequency = 0;

		/// Linked with the frequency, determines the channel frequency.
		int octave = 0;

		/// Sets sets this channel on or off, as an input to the ADSR envelope,
		bool key_on = false;

		/// Sets the degree of feedback applied to the modulator.
		int feedback_strength = 0;

		/// Selects between FM synthesis, using the modulator to modulate the carrier, or simple mixing of the two
		/// underlying operators as completely disjoint entities.
		bool use_fm_synthesis = true;

		// Stored separately because carrier/modulator may not be unique per channel —
		// on the OPLL there's an extra level of indirection.
		OperatorState carrier_state_, modulator_state_;
};

template <typename Child> class OPLBase: public ::Outputs::Speaker::SampleSource {
	public:
		void write(uint16_t address, uint8_t value);

	protected:
		OPLBase(Concurrency::DeferringAsyncTaskQueue &task_queue);

		Concurrency::DeferringAsyncTaskQueue &task_queue_;

		int exponential_[256];
		int log_sin_[256];

		uint8_t depth_rhythm_control_;
		uint8_t csm_keyboard_split_;
		bool waveform_enable_;

	private:
		uint8_t selected_register_ = 0;
};

struct OPL2: public OPLBase<OPL2> {
	public:
		// Creates a new OPL2.
		OPL2(Concurrency::DeferringAsyncTaskQueue &task_queue);

		/// As per ::SampleSource; provides a broadphase test for silence.
		bool is_zero_level();

		/// As per ::SampleSource; provides audio output.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);

		/// Reads from the OPL.
		uint8_t read(uint16_t address);

	private:
		friend OPLBase<OPL2>;

		Operator operators_[18];
		Channel channels_[9];

		// This is the correct LSFR per forums.submarine.org.uk.
		Numeric::LFSR<uint32_t, 0x800302> noise_source_;

		// Synchronous properties, valid only on the emulation thread.
		uint8_t timers_[2] = {0, 0};
		uint8_t timer_control_ = 0;

		void write_register(uint8_t address, uint8_t value);
};

struct OPLL: public OPLBase<OPLL> {
	public:
		// Creates a new OPLL or VRC7.
		OPLL(Concurrency::DeferringAsyncTaskQueue &task_queue, bool is_vrc7 = false);

		/// As per ::SampleSource; provides a broadphase test for silence.
		bool is_zero_level();

		/// As per ::SampleSource; provides audio output.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);

		/// Reads from the OPL.
		uint8_t read(uint16_t address);

	private:
		friend OPLBase<OPLL>;

		Operator operators_[38];	// There's an extra level of indirection with the OPLL; these 38
									// operators are to describe 19 hypothetical channels, being
									// one user-configurable channel, 15 hard-coded channels, and
									// three channels configured for rhythm generation.

		struct Channel: public ::Yamaha::OPL::Channel {
			Operator *modulator;	// Implicitly, the carrier is modulator+1.
			OperatorOverrides overrides;
		};
		Channel channels_[9];

		void setup_fixed_instrument(int number, const uint8_t *data);
		uint8_t custom_instrument_[8];

		void write_register(uint8_t address, uint8_t value);
};

}
}

#endif /* OPL2_hpp */
