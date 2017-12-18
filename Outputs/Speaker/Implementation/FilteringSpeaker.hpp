//
//  FilteringSpeaker.h
//  Clock Signal
//
//  Created by Thomas Harte on 15/12/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef FilteringSpeaker_h
#define FilteringSpeaker_h

#include "../Speaker.hpp"
#include "../../../SignalProcessing/Stepper.hpp"
#include "../../../SignalProcessing/FIRFilter.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../../../Concurrency/AsyncTaskQueue.hpp"

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
		void get_samples(std::size_t number_of_samples, int16_t *target) {}

		/*!
			Should skip the next @c number_of_samples. Subclasses of this SampleSource
			need not implement this if it would no more efficient to do so than it is
			merely to call get_samples and throw the result away, as per the default
			implementation below.
		*/
		void skip_samples(const std::size_t number_of_samples) {
			int16_t scratch_pad[number_of_samples];
			get_samples(number_of_samples, scratch_pad);
		}
};

/*!
	The low-pass speaker
*/
template <typename T> class LowpassSpeaker: public Speaker {
	public:
		LowpassSpeaker(T &sample_source) : sample_source_(sample_source) {}

		// Implemented as per Speaker.
		float get_ideal_clock_rate_in_range(float minimum, float maximum) {
			// return twice the cut off, if applicable
			if(	filter_parameters_.high_frequency_cutoff > 0.0f &&
				filter_parameters_.input_cycles_per_second >= filter_parameters_.high_frequency_cutoff * 3.0f &&
				filter_parameters_.input_cycles_per_second <= filter_parameters_.high_frequency_cutoff * 3.0f)
					return filter_parameters_.high_frequency_cutoff * 3.0f;

			// return exactly the input rate if possible
			if(	filter_parameters_.input_cycles_per_second >= minimum &&
				filter_parameters_.input_cycles_per_second <= maximum)
					return filter_parameters_.input_cycles_per_second;

			// if the input rate is lower, return the minimum
			if(filter_parameters_.input_cycles_per_second < minimum)
				return minimum;

			// otherwise, return the maximum
			return maximum;
		}

		// Implemented as per Speaker.
		void set_output_rate(float cycles_per_second, int buffer_size) {
			filter_parameters_.output_cycles_per_second = cycles_per_second;
			filter_parameters_.parameters_are_dirty = true;
			output_buffer_.resize(static_cast<std::size_t>(buffer_size));
		}

		/*!
			Sets the clock rate of the input audio.
		*/
		void set_input_rate(float cycles_per_second) {
			filter_parameters_.input_cycles_per_second = cycles_per_second;
			filter_parameters_.parameters_are_dirty = true;
		}

		/*!
			Allows a cut-off frequency to be specified for audio. Ordinarily this low-pass speaker
			will determine a cut-off based on the output audio rate. A caller can manually select
			an alternative cut-off. This allows machines with a low-pass filter on their audio output
			path to be explicit about its effect, and get that simulation for free.
		*/
		void set_high_frequency_cutoff(float high_frequency) {
			filter_parameters_.high_frequency_cutoff = high_frequency;
			filter_parameters_.parameters_are_dirty = true;
		}

		/*!
			Advances by the number of cycles specified, obtaining data from the sample source supplied
			at construction, filtering it and passing it on to the speaker's delegate if there is one.
		*/
		void run_for(const Cycles cycles) {
			std::size_t cycles_remaining = static_cast<size_t>(cycles.as_int());
			if(!cycles_remaining) return;
			if(filter_parameters_.parameters_are_dirty) update_filter_coefficients();

			// If input and output rates exactly match, and no additional cut-off has been specified,
			// just accumulate results and pass on.
			if(	filter_parameters_.input_cycles_per_second == filter_parameters_.output_cycles_per_second &&
				filter_parameters_.high_frequency_cutoff < 0.0) {
				while(cycles_remaining) {
					std::size_t cycles_to_read = std::min(output_buffer_.size() - output_buffer_pointer_, cycles_remaining);

					sample_source_.get_samples(cycles_to_read, &output_buffer_[output_buffer_pointer_]);
					output_buffer_pointer_ += cycles_to_read;

					// announce to delegate if full
					if(output_buffer_pointer_ == output_buffer_.size()) {
						output_buffer_pointer_ = 0;
						if(delegate_) {
							delegate_->speaker_did_complete_samples(this, output_buffer_);
						}
					}

					cycles_remaining -= cycles_to_read;
				}

				return;
			}

			// if the output rate is less than the input rate, or an additional cut-off has been specified, use the filter.
			if(	filter_parameters_.input_cycles_per_second > filter_parameters_.output_cycles_per_second ||
				(filter_parameters_.input_cycles_per_second == filter_parameters_.output_cycles_per_second && filter_parameters_.high_frequency_cutoff >= 0.0)) {
				while(cycles_remaining) {
					std::size_t cycles_to_read = std::min(cycles_remaining, input_buffer_.size() - input_buffer_depth_);
					sample_source_.get_samples(cycles_to_read, &input_buffer_[input_buffer_depth_]);
					cycles_remaining -= cycles_to_read;
					input_buffer_depth_ += cycles_to_read;

					if(input_buffer_depth_ == input_buffer_.size()) {
						output_buffer_[output_buffer_pointer_] = filter_->apply(input_buffer_.data());
						output_buffer_pointer_++;

						// Announce to delegate if full.
						if(output_buffer_pointer_ == output_buffer_.size()) {
							output_buffer_pointer_ = 0;
							if(delegate_) {
								delegate_->speaker_did_complete_samples(this, output_buffer_);
							}
						}

						// If the next loop around is going to reuse some of the samples just collected, use a memmove to
						// preserve them in the correct locations (TODO: use a longer buffer to fix that) and don't skip
						// anything. Otherwise skip as required to get to the next sample batch and don't expect to reuse.
						uint64_t steps = stepper_->step();
						if(steps < input_buffer_.size()) {
							int16_t *input_buffer = input_buffer_.data();
							memmove(	input_buffer,
										&input_buffer[steps],
										sizeof(int16_t) * (input_buffer_.size() - steps));
							input_buffer_depth_ -= steps;
						} else {
							if(steps > input_buffer_.size())
								sample_source_.skip_samples(steps - input_buffer_.size());
							input_buffer_depth_ = 0;
						}
					}
				}

				return;
			}

			// TODO: input rate is less than output rate
		}

		/*!
			Provides a convenience shortcut for deferring a call to run_for.
		*/
		void run_for(Concurrency::DeferringAsyncTaskQueue &queue, const Cycles cycles) {
			queue.defer([this, cycles] {
				run_for(cycles);
			});
		}

	private:
		T &sample_source_;

		std::size_t output_buffer_pointer_ = 0;
		std::size_t input_buffer_depth_ = 0;
		std::vector<int16_t> input_buffer_;
		std::vector<int16_t> output_buffer_;

		std::unique_ptr<SignalProcessing::Stepper> stepper_;
		std::unique_ptr<SignalProcessing::FIRFilter> filter_;

		struct FilterParameters {
			float input_cycles_per_second = 0.0f;
			float output_cycles_per_second = 0.0f;
			float high_frequency_cutoff = -1.0;

			bool parameters_are_dirty = true;
		} filter_parameters_;

		void update_filter_coefficients() {
			// Make a guess at a good number of taps.
			std::size_t number_of_taps = static_cast<std::size_t>(
				ceilf((filter_parameters_.input_cycles_per_second + filter_parameters_.output_cycles_per_second) / filter_parameters_.output_cycles_per_second)
			);
			number_of_taps = (number_of_taps * 2) | 1;

			filter_parameters_.parameters_are_dirty = false;
			output_buffer_pointer_ = 0;

			stepper_.reset(new SignalProcessing::Stepper(
				static_cast<uint64_t>(filter_parameters_.input_cycles_per_second),
				static_cast<uint64_t>(filter_parameters_.output_cycles_per_second)));

			float high_pass_frequency = filter_parameters_.output_cycles_per_second / 2.0f;
			if(filter_parameters_.high_frequency_cutoff > 0.0) {
				high_pass_frequency = std::min(filter_parameters_.output_cycles_per_second / 2.0f, high_pass_frequency);
			}
			filter_.reset(new SignalProcessing::FIRFilter(
				static_cast<unsigned int>(number_of_taps),
				filter_parameters_.input_cycles_per_second,
				0.0,
				high_pass_frequency,
				SignalProcessing::FIRFilter::DefaultAttenuation));

			input_buffer_.resize(static_cast<std::size_t>(number_of_taps));
			input_buffer_depth_ = 0;
		}
};

}
}

#endif /* FilteringSpeaker_h */
