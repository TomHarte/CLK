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

#include "Implementation/Channel.hpp"
#include "Implementation/Operator.hpp"

#include <atomic>

namespace Yamaha {
namespace OPL {

/*
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
			void update(const LowFrequencyOscillator &oscillator) {
				Yamaha::OPL::Channel::update(true, oscillator, modulator[0]);
				Yamaha::OPL::Channel::update(false, oscillator, modulator[1], false, &overrides);
			}

			void update(const LowFrequencyOscillator &oscillator, Operator *mod, bool key_on) {
				Yamaha::OPL::Channel::update(true, oscillator, mod[0], key_on);
				Yamaha::OPL::Channel::update(false, oscillator, mod[1], key_on, &overrides);
			}

			using ::Yamaha::OPL::Channel::update;

			int melodic_output() {
				return Yamaha::OPL::Channel::melodic_output(modulator[0], modulator[1], &overrides);
			}

			int melodic_output(const OperatorOverrides *overrides) {
				return Yamaha::OPL::Channel::melodic_output(modulator[0], modulator[1], overrides);
			}

			bool is_audible() {
				return Yamaha::OPL::Channel::is_audible(modulator + 1, &overrides);
			}

			Operator *modulator;	// Implicitly, the carrier is modulator+1.
			OperatorOverrides overrides;
		};
		void update_all_chanels();
		Channel channels_[9];
		int output_levels_[18];
		OperatorOverrides rhythm_overrides_[6];

		void setup_fixed_instrument(int number, const uint8_t *data);
		uint8_t custom_instrument_[8];

		void write_register(uint8_t address, uint8_t value);

		const int audio_divider_ = 1;
		int audio_offset_ = 0;

		std::atomic<int> total_volume_;
};*/

}
}

#endif /* OPL2_hpp */
