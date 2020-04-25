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

#include "Implementation/Channel.hpp"
#include "Implementation/Operator.hpp"

#include <atomic>

namespace Yamaha {
namespace OPL {

template <typename Child> class OPLBase: public ::Outputs::Speaker::SampleSource {
	public:
		void write(uint16_t address, uint8_t value);

	protected:
		OPLBase(Concurrency::DeferringAsyncTaskQueue &task_queue);

		Concurrency::DeferringAsyncTaskQueue &task_queue_;
		LowFrequencyOscillator oscillator_;

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
			int update_melodic(const LowFrequencyOscillator &oscillator) {
				return Yamaha::OPL::Channel::update_melodic(oscillator, modulator, modulator + 1, false, nullptr, &overrides);
			}

			int update_bass(const LowFrequencyOscillator &oscillator, Operator *bass, bool key_on) {
				return Yamaha::OPL::Channel::update_melodic(oscillator, bass, bass + 1, key_on, nullptr, &overrides);
			}

			int update_tom_tom(const LowFrequencyOscillator &oscillator, Operator *bass, bool key_on) {
				// TODO: should overrides be applied here?
				return Yamaha::OPL::Channel::update_tom_tom(oscillator, bass, key_on, &overrides);
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
