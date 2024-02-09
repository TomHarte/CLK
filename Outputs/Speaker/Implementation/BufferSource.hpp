//
//  SampleSource.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../Speaker.hpp"

namespace Outputs::Speaker {

/*!
	A sample source is something that can provide a stream of audio.
	This optional base class provides the interface expected to be exposed
	by the template parameter to LowpassSpeaker.
*/
template <typename SourceT, bool stereo>
class BufferSource {
	public:
		/*!
			Indicates whether this component will write stereo samples.
		*/
		static constexpr bool is_stereo = stereo;

		/*!
			Should write the next @c number_of_samples to @c target.
		*/
		void get_samples([[maybe_unused]] std::size_t number_of_samples, [[maybe_unused]] typename SampleT<stereo>::type *target) {}

		/*!
			Should skip the next @c number_of_samples. Subclasses of this SampleSource
			need not implement this if it would no more efficient to do so than it is
			merely to call get_samples and throw the result away, as per the default
			implementation below.
		*/
		void skip_samples(std::size_t number_of_samples) {
			typename SampleT<stereo>::type scratch_pad[number_of_samples];
			get_samples(number_of_samples, scratch_pad);
		}

		/*!
			@returns @c true if it is trivially true that a call to get_samples would just
				fill the target with zeroes; @c false if a call might return all zeroes or
				might not.
		*/
		bool is_zero_level() const						{	return false;	}

		/*!
			Sets the proper output range for this sample source; it should write values
			between 0 and volume.
		*/
		void set_sample_volume_range([[maybe_unused]] std::int16_t volume) {}

		/*!
			Permits a sample source to declare that, averaged over time, it will use only
			a certain proportion of the allocated volume range. This commonly happens
			in sample sources that use a time-multiplexed sound output — for example, if
			one were to output only every other sample then it would return 0.5.

			This is permitted to vary over time but there is no contract as to when it will be
			used by a speaker. If it varies, it should do so very infrequently and only to
			represent changes in hardware configuration.
		*/
		double average_output_peak() const { return 1.0; }
};

///
template <typename SourceT, bool stereo, int divider = 1>
struct SampleSource: public BufferSource<SourceT, stereo> {
	public:
		void get_samples(std::size_t number_of_samples, typename SampleT<stereo>::type *target) {
			const auto &source = *static_cast<SourceT *>(this);

			if constexpr (divider == 1) {
				while(number_of_samples--) {
					*target = source.level();
					++target;
				}
			} else {
				std::size_t c = 0;

				// Fill in the tail of any partially-captured level.
				auto level = source.level();
				while(c < number_of_samples && master_divider_ != divider) {
					target[c] = level;
					++c;
					++master_divider_;
				}
				source.advance();

				// Provide all full levels.
				int whole_steps = (number_of_samples - c) / divider;
				while(whole_steps--) {
					std::fill(&target[c], &target[c + divider], source.level());
					c += divider;
					source.advance();
				}

				// Provide the head of a further partial capture.
				level = source.level();
				master_divider_ = number_of_samples - c;
				std::fill(&target[c], &target[number_of_samples], source.level());
			}
		}

		void skip_samples(std::size_t number_of_samples) {
			const auto &source = *static_cast<SourceT *>(this);

			if constexpr (&SourceT::advance == &SampleSource::advance) {
				return;
			}

			if constexpr (divider == 1) {
				while(--number_of_samples) {
					source.advance();
				}
			} else {
				if(number_of_samples >= divider - master_divider_) {
					source.advance();
					number_of_samples -= (divider - master_divider_);
				}
				while(number_of_samples > divider) {
					advance();
					number_of_samples -= divider;
				}
				master_divider_ = number_of_samples;
			}
		}

		// TODO: use a concept here, when C++20 filters through.
		//
		// Until then: sample sources should implement this.
		auto level() const	{
			return typename SampleT<stereo>::type();
		}
		void advance() {}

	private:
		int master_divider_{};
};

}
