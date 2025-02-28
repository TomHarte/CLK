//
//  SampleSource.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Outputs/Speaker/Speaker.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Outputs::Speaker {

enum class Action {
	/// New values should be _stored_ to the sample buffer.
	Store,
	/// New values should be _added_ to the sample buffer.
	Mix,
	/// New values shouldn't be stored; the source can skip generation of them if desired.
	Ignore,
};

template <Action action, typename SampleT> void apply(SampleT &lhs, SampleT rhs) {
	switch(action) {
		case Action::Mix:	lhs += rhs;	break;
		case Action::Store:	lhs = rhs;	break;
		case Action::Ignore:			break;
	}
}

template <Action action, typename IteratorT, typename SampleT> void fill(IteratorT begin, IteratorT end, SampleT value) {
	switch(action) {
		case Action::Mix:
			while(begin != end) {
				apply<action>(*begin, value);
				++begin;
			}
		break;
		case Action::Store:
			std::fill(begin, end, value);
		break;
		case Action::Ignore:	break;
	}
}

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
			Should 'apply' the next @c number_of_samples to @c target ; application means applying @c action which can be achieved either via the
			helper functions above — @c apply and @c fill — or by semantic inspection (primarily, if an obvious quick route for @c Action::Ignore is available).

			No default implementation is provided.
		*/
		template <Action action>
		void apply_samples(std::size_t number_of_samples, typename SampleT<stereo>::type *target);

		/*!
			@returns @c true if it is trivially true that a call to get_samples would just
				fill the target with zeroes; @c false if a call might return all zeroes or
				might not.
		*/
//		bool is_zero_level() const						{	return false;	}

		/*!
			Sets the proper output range for this sample source; it should write values
			between 0 and volume.
		*/
//		void set_sample_volume_range(std::int16_t volume);

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
	template <Action action>
	void apply_samples(std::size_t number_of_samples, typename SampleT<stereo>::type *target) {
		auto &source = *static_cast<SourceT *>(this);

		if constexpr (divider == 1) {
			while(number_of_samples--) {
				apply<action>(*target, source.level());
				++target;
				source.advance();
			}
		} else {
			std::size_t c = 0;

			// Fill in the tail of any partially-captured level.
			auto level = source.level();
			while(c < number_of_samples && master_divider_ != divider) {
				apply<action>(target[c], level);
				++c;
				++master_divider_;
			}
			source.advance();

			// Provide all full levels.
			auto whole_steps = static_cast<int>((number_of_samples - c) / divider);
			while(whole_steps--) {
				fill<action>(&target[c], &target[c + divider], source.level());
				c += divider;
				source.advance();
			}

			// Provide the head of a further partial capture.
			level = source.level();
			master_divider_ = static_cast<int>(number_of_samples - c);
			fill<action>(&target[c], &target[number_of_samples], source.level());
		}
	}

	// TODO: use a concept here, when C++20 filters through.
	//
	// Until then: sample sources should implement this.
//	typename SampleT<stereo>::type level() const;
//	void advance();

private:
	int master_divider_{};
};

}
