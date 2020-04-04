//
//  SampleSource.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef SampleSource_hpp
#define SampleSource_hpp

#include <cstddef>
#include <cstdint>

namespace Outputs {
namespace Speaker {

/*!
	A sample source is something that can provide a stream of audio.
	This optional base class provides the interface expected to be exposed
	by the template parameter to LowpassSpeaker.
*/
class SampleSource {
	public:
		/*!
			Should write the next @c number_of_samples to @c target.
		*/
		void get_samples(std::size_t number_of_samples, std::int16_t *target) {}

		/*!
			Should skip the next @c number_of_samples. Subclasses of this SampleSource
			need not implement this if it would no more efficient to do so than it is
			merely to call get_samples and throw the result away, as per the default
			implementation below.
		*/
		void skip_samples(const std::size_t number_of_samples) {
			std::int16_t scratch_pad[number_of_samples];
			get_samples(number_of_samples, scratch_pad);
		}

		/*!
			@returns @c true if it is trivially true that a call to get_samples would just
				fill the target with zeroes; @c false if a call might return all zeroes or
				might not.
		*/
		bool is_zero_level() {
			return false;
		}

		/*!
			Sets the proper output range for this sample source; it should write values
			between 0 and volume.
		*/
		void set_sample_volume_range(std::int16_t volume) {
		}

		/*!
			Indicates whether this component will write stereo samples.
		*/
		static constexpr bool get_is_stereo() { return false; }
};

}
}

#endif /* SampleSource_hpp */
