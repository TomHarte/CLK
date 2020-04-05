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

namespace Yamaha {

/*!
	Provides an emulation of the OPL2 core, along with its OPLL and VRC7 specialisations.
*/
class OPL2: public ::Outputs::Speaker::SampleSource {
	public:
		enum class Personality {
			OPL2,	// Provides full configuration of all channels.
			OPLL,	// i.e. YM2413; uses the OPLL sound font, permitting full configuration of only a single channel.
			VRC7,	// Uses the VRC7 sound font, permitting full configuration of only a single channel.
		};

		// Creates a new OPL2, OPLL or VRC7.
		OPL2(Personality personality, Concurrency::DeferringAsyncTaskQueue &task_queue);

		/// As per ::SampleSource; provides a broadphase test for silence.
		bool is_zero_level();

		/// As per ::SampleSource; provides audio output.
		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);

		/// Writes to the OPL.
		void write(uint16_t address, uint8_t value);

		/// Reads from the OPL.
		uint8_t read(uint16_t address);

	private:
		Concurrency::DeferringAsyncTaskQueue &task_queue_;

		int exponential_[256];
		int log_sin_[256];
		uint8_t selected_register_ = 0;

		void set_opl2_register(uint8_t location, uint8_t value);

		// Asynchronous properties, valid only on the audio thread.
		struct Operator {
			bool apply_amplitude_modulation = false;
			bool apply_vibrato = false;
			bool hold_sustain_level = false;
			bool keyboard_scaling_rate = false;	// ???
			int frequency_multiple = 0;
			int scaling_level = 0;
			int output_level = 0;
			int attack_rate = 0;
			int decay_rate = 0;
			int sustain_level = 0;
			int release_rate = 0;
			int waveform = 0;
		} operators_[22];

		struct Channel {
			int frequency;
			int octave;
			bool key_on;
			int feedback_strength;
			bool two_operator;
		} channels_[9];

		uint8_t depth_rhythm_control_;
		uint8_t csm_keyboard_split_;
		bool waveform_enable_;

		// Synchronous properties, valid only on the emulation thread.
		uint8_t timers_[2];
		uint8_t timer_control_;
};

}

#endif /* OPL2_hpp */
