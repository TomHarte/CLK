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
	bool apply_amplitude_modulation = false;
	bool apply_vibrato = false;
	bool hold_sustain_level = false;
	bool keyboard_scaling_rate = false;
	int frequency_multiple = 0;
	int scaling_level = 0;
	int output_level = 0;
	int attack_rate = 0;
	int decay_rate = 0;
	int sustain_level = 0;
	int release_rate = 0;
	int waveform = 0;
};

struct Channel {
	int frequency = 0;
	int octave = 0;
	bool key_on = false;
	int feedback_strength = 0;
	bool use_fm_synthesis = true;
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
