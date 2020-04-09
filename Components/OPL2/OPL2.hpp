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

struct Operator {
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

struct Channel {
	int frequency = 0;
	int octave = 0;
	bool key_on = false;
	int feedback_strength = 0;
	bool use_fm_synthesis = true;

	// This should be called at a rate of around 49,716 Hz.
	void update() {
		// Per the documentation:
		// F-Num = Music Frequency * 2^(20-Block) / 49716
		//
		// Given that a 256-entry table is used to store a quarter of a sine wave,
		// making 1024 steps per complete wave, add what I've called frequency
		// to an accumulator and move on whenever that exceeds 2^(10 - octave).
		//
		// TODO: but, how does that apply to the two operator multipliers?
		//
		// Or: 2^19?
	}

	// Stateful information.
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

		Operator operators_[32];
		struct Channel: public ::Yamaha::OPL::Channel {
			int output_level = 0;
			bool hold_sustain_level = false;
			Operator *modulator;	// Implicitly, the carrier is modulator+1.
		};
		Channel channels_[9];

		void setup_fixed_instrument(int number, const uint8_t *data);
		uint8_t custom_instrument_[8];

		void write_register(uint8_t address, uint8_t value);
};

}
}

#endif /* OPL2_hpp */
