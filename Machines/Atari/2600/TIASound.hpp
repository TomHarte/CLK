//
//  TIASound.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Atari2600_TIASound_hpp
#define Atari2600_TIASound_hpp

#include "../../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../../Concurrency/AsyncTaskQueue.hpp"

namespace Atari2600 {

// This should be a divisor of 38; audio counters are updated every 38 cycles, though lesser dividers
// will give greater resolution to changes in audio state. 1, 2 and 19 are the only divisors of 38.
const int CPUTicksPerAudioTick = 2;

class TIASound: public Outputs::Speaker::SampleSource {
	public:
		TIASound(Concurrency::DeferringAsyncTaskQueue &audio_queue);

		void set_volume(int channel, uint8_t volume);
		void set_divider(int channel, uint8_t divider);
		void set_control(int channel, uint8_t control);

		// To satisfy ::SampleSource.
		void get_samples(std::size_t number_of_samples, int16_t *target);
		void set_sample_volume_range(std::int16_t range);

	private:
		Concurrency::DeferringAsyncTaskQueue &audio_queue_;

		uint8_t volume_[2];
		uint8_t divider_[2];
		uint8_t control_[2];

		int poly4_counter_[2];
		int poly5_counter_[2];
		int poly9_counter_[2];
		int output_state_[2];

		int divider_counter_[2];
		int16_t per_channel_volume_ = 0;
};

}

#endif /* Speaker_hpp */
