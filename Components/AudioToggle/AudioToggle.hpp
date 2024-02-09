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

		void get_samples(std::size_t number_of_samples, Outputs::Speaker::MonoSample *target);
		void set_sample_volume_range(std::int16_t range);
		void skip_samples(const std::size_t number_of_samples);

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
