//
//  AudioToggle.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Speaker/Implementation/BufferSource.hpp"
#include "Concurrency/AsyncTaskQueue.hpp"

namespace Audio {

/*!
	Provides a sample source that can programmatically be set to a fixed value
*/
class DAC: public Outputs::Speaker::BufferSource<DAC, false> {
public:
	DAC(Concurrency::AsyncTaskQueue<false> &audio_queue, int16_t max);

	template <Outputs::Speaker::Action action>
	void apply_samples(const std::size_t number_of_samples, Outputs::Speaker::MonoSample *const target) {
		Outputs::Speaker::fill<action>(target, target + number_of_samples, level_);
	}
	void set_sample_volume_range(const std::int16_t range);
	bool is_zero_level() const {
		return !level_;
	}

	void set_output(int16_t);
	int16_t get_output() const;

private:
	// Accessed on the calling thread.
	int16_t set_output_ = 0;
	Concurrency::AsyncTaskQueue<false> &audio_queue_;

	// Accessed on the audio thread.
	int16_t level_ = 0, volume_ = 0;
	int16_t output_ = 0, max_output_ = 0;
	void update_level();
};

/*!
	Provides a 1-bit specialisation of the DAC.
*/
struct Toggle: public DAC {
	Toggle(Concurrency::AsyncTaskQueue<false> &audio_queue) :
		DAC(audio_queue, 1) {}

	void set_output(const bool enabled) {
		DAC::set_output(enabled);
	}
	bool get_output() const {
		return bool(DAC::get_output());
	}
};

}
