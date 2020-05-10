//
//  AudioToggle.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef AudioToggle_hpp
#define AudioToggle_hpp

#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace Audio {

/*!
	Provides a sample source that can programmatically be set to one of two values.
*/
class Toggle: public Outputs::Speaker::SampleSource {
	public:
		Toggle(Concurrency::DeferringAsyncTaskQueue &audio_queue);

		void get_samples(std::size_t number_of_samples, std::int16_t *target);
		void set_sample_volume_range(std::int16_t range);
		void skip_samples(const std::size_t number_of_samples);

		void set_output(bool enabled);
		bool get_output() const;

	private:
		// Accessed on the calling thread.
		bool is_enabled_ = false;
		Concurrency::DeferringAsyncTaskQueue &audio_queue_;

		// Accessed on the audio thread.
		int16_t level_ = 0, volume_ = 0;
};

}

#endif /* AudioToggle_hpp */
