//
//  Audio.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/05/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Audio_hpp
#define Audio_hpp

#include "../../../Concurrency/AsyncTaskQueue.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Outputs/Speaker/Implementation/SampleSource.hpp"

#include <array>
#include <atomic>

namespace Apple {
namespace Macintosh {

/*!
	Implements the Macintosh's audio output hardware, using a
	combination
*/
class Audio: public ::Outputs::Speaker::SampleSource {
	public:
		Audio(Concurrency::DeferringAsyncTaskQueue &task_queue);

		/*!
			Macintosh audio is (partly) sourced by the same scanning
			hardware as the video; each line it collects an additional
			word of memory, half of which is used for audio output.

			Use this method to add a newly-collected sample to the queue.
		*/
		void post_sample(uint8_t sample);

		/*!
			Macintosh audio also separately receives an output volume
			level, in the range 0 to 7.

			Use this method to set the current output volume.
		*/
		void set_volume(int volume);

		/*!
			A further factor in audio output is the on-off toggle.
		*/
		void set_enabled(bool on);

		// to satisfy ::Outputs::Speaker (included via ::Outputs::Filter.
		void get_samples(std::size_t number_of_samples, int16_t *target);
		void skip_samples(std::size_t number_of_samples);
		bool is_zero_level();
		void set_sample_volume_range(std::int16_t range);

	private:
		Concurrency::DeferringAsyncTaskQueue &task_queue_;

		// A queue of fetched samples; read from by one thread,
		// written to by another.
		struct {
			std::array<uint8_t, 2048> buffer;
			std::atomic<unsigned int> read_pointer, write_pointer;
		} sample_queue_;

		// Stateful variables, modified from the audio generation
		// thread only.
		int volume_ = 0;
		int enabled_mask_ = 0;

		std::int16_t volume_multiplier_ = 0;

		std::size_t subcycle_offset_;
};

}
}

#endif /* Audio_hpp */
