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

#include <atomic>
#include <cmath>

namespace Yamaha {


namespace OPL {

/*!
	Describes the ephemeral state of an operator.
*/
struct OperatorState {
	public:
		int phase = 0;			// Will be in the range [0, 1023], mapping into a 1024-unit sine curve.
		int attenuation = 255;	// Will be in the range [0, 1023].

	private:
		int divider_ = 0;
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

	Oscillator period isn't set directly, it's a multiple of the owning channel, in which
	period is set as a combination of f-num and octave.
*/
class Operator {
	public:
		/// Sets this operator's attack rate as the top nibble of @c value, its decay rate as the bottom nibble.
		void set_attack_decay(uint8_t value) {
			attack_rate_ = (value & 0xf0) >> 2;
			decay_rate_ = (value & 0x0f) << 2;
		}

		/// Sets this operator's sustain level as the top nibble of @c value, its release rate as the bottom nibble.
		void set_sustain_release(uint8_t value) {
			sustain_level_ = (value & 0xf0) >> 4;
			release_rate_ = (value & 0x0f) << 2;
		}

		/// Sets this operator's key scale level as the top two bits of @c value, its total output level as the low six bits.
		void set_scaling_output(uint8_t value) {
			scaling_level = value >> 6;
			attenuation_ = value & 0x3f;
		}

		/// Sets this operator's waveform using the low two bits of @c value.
		void set_waveform(uint8_t value) {
//			waveform = Operator::Waveform(value & 3);
		}

		/// From the top nibble of @c value sets the AM, vibrato, hold/sustain level and keyboard sampling rate flags;
		/// uses the bottom nibble to set the period multiplier.
		void set_am_vibrato_hold_sustain_ksr_multiple(uint8_t value) {
			apply_amplitude_modulation = value & 0x80;
			apply_vibrato = value & 0x40;
			hold_sustain_level = value & 0x20;
			keyboard_scaling_rate = value & 0x10;
			frequency_multiple = value & 0xf;
		}

		void update(OperatorState &state, bool key_on, int channel_period, int channel_octave, OperatorOverrides *overrides = nullptr);

		bool is_audible(OperatorState &state, OperatorOverrides *overrides = nullptr) {
			if(state.adsr_phase_ == OperatorState::ADSRPhase::Release) {
				if(overrides) {
					if(overrides->attenuation == 0xf) return false;
				} else {
					if(attenuation_ == 0x3f) return false;
				}
			}
			return state.adsr_attenuation_ != 511;
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
		int attenuation_ = 0;

		/// Selects attenuation that is applied as a function of interval. Cf. p14.
		int scaling_level = 0;

		/// Sets the ADSR rates. These all provide the top four bits of a six-bit number;
		/// the bottom two bits... are 'RL'?
		int attack_rate_ = 0;
		int decay_rate_ = 0;
		int sustain_level_ = 0;
		int release_rate_ = 0;

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
			period_ = (period_ &~0xff) | value;
		}

		/// Sets the high two bits of a 10-bit frequency control, along with this channel's
		/// block/octave, and key on or off.
		void set_10bit_frequency_octave_key_on(uint8_t value) {
			period_ = (period_ & 0xff) | ((value & 3) << 8);
			octave = (value >> 2) & 0x7;
			key_on = value & 0x20;
			frequency_shift = 0;
		}

		/// Sets the high two bits of a 9-bit frequency control, along with this channel's
		/// block/octave, and key on or off.
		void set_9bit_frequency_octave_key_on(uint8_t value) {
			period_ = (period_ & 0xff) | ((value & 1) << 8);
			octave = (value >> 1) & 0x7;
			key_on = value & 0x10;;
			frequency_shift = 1;
		}

		/// Sets the amount of feedback provided to the first operator (i.e. the modulator)
		/// associated with this channel, and whether FM synthesis is in use.
		void set_feedback_mode(uint8_t value) {
			feedback_strength = (value >> 1) & 0x7;
			use_fm_synthesis = value & 1;
		}

		/// This should be called at a rate of around 49,716 Hz; it returns the current output level
		/// level for this channel.
		int update(Operator *modulator, Operator *carrier, OperatorOverrides *modulator_overrides = nullptr, OperatorOverrides *carrier_overrides = nullptr) {
			modulator->update(modulator_state_, key_on, period_ << frequency_shift, octave, modulator_overrides);
			carrier->update(carrier_state_, key_on, period_ << frequency_shift, octave, carrier_overrides);

			// TODO: almost everything. This is a quick test.
			// Specifically: use lookup tables.
			const auto modulator_level = 0.0f;//level(modulator_state_, 0.0f) * 0.25f;
			return int(level(carrier_state_, modulator_level) * 20'000.0f);
		}

		/// @returns @c true if this channel is currently producing any audio; @c false otherwise;
		bool is_audible(Operator *carrier, OperatorOverrides *carrier_overrides = nullptr) {
			return carrier->is_audible(carrier_state_, carrier_overrides);
		}

	private:
		float level(OperatorState &state, float modulator_level) {
			const float phase = modulator_level + float(state.phase) / 1024.0f;
			const float phase_attenuation = logf(1.0f + sinf(float(M_PI) * 2.0f * phase));
			const float total_attenuation = phase_attenuation + float(state.attenuation) / 1023.0f;
			return expf(total_attenuation);
		}

		/// 'F-Num' in the spec; this plus the current octave determines channel frequency.
		int period_ = 0;

		/// Linked with the frequency, determines the channel frequency.
		int octave = 0;

		/// Sets sets this channel on or off, as an input to the ADSR envelope,
		bool key_on = false;

		/// Sets the degree of feedback applied to the modulator.
		int feedback_strength = 0;

		/// Selects between FM synthesis, using the modulator to modulate the carrier, or simple mixing of the two
		/// underlying operators as completely disjoint entities.
		bool use_fm_synthesis = true;

		/// Used internally to make both the 10-bit OPL2 frequency selection and 9-bit OPLL/VRC7 frequency
		/// selections look the same when passed to the operators.
		int frequency_shift = 0;

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
		OPLL(Concurrency::DeferringAsyncTaskQueue &task_queue, int audio_divider = 1, bool is_vrc7 = false);

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
			int update() {
				return Yamaha::OPL::Channel::update(modulator, modulator + 1, nullptr, &overrides);
			}

			bool is_audible() {
				return Yamaha::OPL::Channel::is_audible(modulator + 1, &overrides);
			}

			Operator *modulator;	// Implicitly, the carrier is modulator+1.
			OperatorOverrides overrides;
			int level = 0;
		};
		void update_all_chanels() {
			for(int c = 0; c < 6; ++ c) {	// Don't do anything with channels that might be percussion for now.
				channels_[c].level = (channels_[c].update() * total_volume_) >> 14;
			}
		}
		Channel channels_[9];

		void setup_fixed_instrument(int number, const uint8_t *data);
		uint8_t custom_instrument_[8];

		void write_register(uint8_t address, uint8_t value);

		const int audio_divider_ = 1;
		int audio_offset_ = 0;

		std::atomic<int> total_volume_;
};

}
}

#endif /* OPL2_hpp */
