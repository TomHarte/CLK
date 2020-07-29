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
	Implements the Macintosh's audio output hardware.

	Designed to be clocked at half the rate of the real hardware — i.e.
	a shade less than 4Mhz.
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
		bool is_zero_level() const;
		void set_sample_volume_range(std::int16_t range);
		constexpr static bool get_is_stereo() { return false; }

	private:
		Concurrency::DeferringAsyncTaskQueue &task_queue_;

		// A queue of fetched samples; read from by one thread,
		// written to by another.
		struct {
			std::array<std::atomic<uint8_t>, 740> buffer;
			size_t read_pointer = 0, write_pointer = 0;
		} sample_queue_;

		// Emulator-thread stateful variables, to avoid work posting
		// deferral updates if possible.
		int posted_volume_ = 0;
		int posted_enable_mask_ = 0;

		// Stateful variables, modified from the audio generation
		// thread only.
		int volume_ = 0;
		int enabled_mask_ = 0;
		std::int16_t output_volume_ = 0;

		std::int16_t volume_multiplier_ = 0;
		std::size_t subcycle_offset_ = 0;
		void set_volume_multiplier();
};

}
}

#endif /* Audio_hpp */
