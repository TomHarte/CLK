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

namespace Outputs::Speaker {

template <bool stereo> struct SampleT;
template <> struct SampleT<true> { using type = std::array<std::int16_t, 2>; };
template <> struct SampleT<false> { using type = std::int16_t; };

/*!
	A sample source is something that can provide a stream of audio.
	This optional base class provides the interface expected to be exposed
	by the template parameter to LowpassSpeaker.
*/
template <typename SourceT>
class SampleSource {
	public:
		/*!
			Indicates whether this component will write stereo samples.
		*/
		static constexpr bool is_stereo = SourceT::is_stereo;

		/*!
			Should write the next @c number_of_samples to @c target.
		*/
		void get_samples(std::size_t number_of_samples, std::int16_t *target) {
			const auto &source = *static_cast<SourceT *>(this);
			while(number_of_samples--) {
				if constexpr (is_stereo) {
					const auto next = source.level();
					target[0] = next[0];
					target[1] = next[1];
					target += 2;
				} else {
					*target = source.level();
					++target;
				}
			}
		}

		/*!
			Should skip the next @c number_of_samples. Subclasses of this SampleSource
			need not implement this if it would no more efficient to do so than it is
			merely to call get_samples and throw the result away, as per the default
			implementation below.
		*/
		void skip_samples(const std::size_t number_of_samples) {
			if constexpr (&SourceT::advance == &SampleSource<SourceT>::advance) {
				return;
			}
			std::int16_t scratch_pad[number_of_samples];
			get_samples(number_of_samples, scratch_pad);
		}

		/*!
			@returns @c true if it is trivially true that a call to get_samples would just
				fill the target with zeroes; @c false if a call might return all zeroes or
				might not.
		*/
		bool is_zero_level() const						{	return false;	}
		auto level() const	{
			typename SampleT<is_stereo>::type result;
			if constexpr (is_stereo) {
				result[0] = result[1] = 0;
			} else {
				result = 0;
			}
			return result;
		}
		void advance()									{}

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

}
