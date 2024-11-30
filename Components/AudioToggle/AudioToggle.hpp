//
//  AudioToggle.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Outputs/Speaker/Implementation/BufferSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace Audio {

/*!
	Provides a sample source that can programmatically be set to one of two values.
*/
class Toggle: public Outputs::Speaker::BufferSource<Toggle, false> {
public:
	Toggle(Concurrency::AsyncTaskQueue<false> &audio_queue);

	template <Outputs::Speaker::Action action>
	void apply_samples(const std::size_t number_of_samples, Outputs::Speaker::MonoSample *const target) {
		Outputs::Speaker::fill<action>(target, target + number_of_samples, level_);
	}
	void set_sample_volume_range(const std::int16_t range);
	bool is_zero_level() const {
		return !level_;
	}

	void set_output(bool enabled);
	bool get_output() const;

private:
	// Accessed on the calling thread.
	bool is_enabled_ = false;
	Concurrency::AsyncTaskQueue<false> &audio_queue_;

	// Accessed on the audio thread.
	int16_t level_ = 0, volume_ = 0;
	bool level_active_ = false;
};

}
