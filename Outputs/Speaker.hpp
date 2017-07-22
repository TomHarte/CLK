//
//  Speaker.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Speaker_hpp
#define Speaker_hpp

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <memory>
#include <list>
#include <vector>

#include "../SignalProcessing/Stepper.hpp"
#include "../SignalProcessing/FIRFilter.hpp"
#include "../Concurrency/AsyncTaskQueue.hpp"

namespace Outputs {

/*!
	Provides the base class for an audio output source, with an input rate (the speed at which the source will
	provide data), an output rate (the speed at which the destination will receive data), a delegate to receive
	the output and some help for the output in picking an appropriate rate once the input rate is known.

	Intended to be a parent class, allowing descendants to pick the strategy by which input samples are mapped to
	output samples.
*/
class Speaker {
	public:
		class Delegate {
			public:
				virtual void speaker_did_complete_samples(Speaker *speaker, const std::vector<int16_t> &buffer) = 0;
		};

		float get_ideal_clock_rate_in_range(float minimum, float maximum) {
			// return twice the cut off, if applicable
			if(high_frequency_cut_off_ > 0.0f && input_cycles_per_second_ >= high_frequency_cut_off_ * 3.0f && input_cycles_per_second_ <= high_frequency_cut_off_ * 3.0f) return high_frequency_cut_off_ * 3.0f;

			// return exactly the input rate if possible
			if(input_cycles_per_second_ >= minimum && input_cycles_per_second_ <= maximum) return input_cycles_per_second_;

			// if the input rate is lower, return the minimum
			if(input_cycles_per_second_ < minimum) return minimum;

			// otherwise, return the maximum
			return maximum;
		}

		void set_output_rate(float cycles_per_second, int buffer_size) {
			output_cycles_per_second_ = cycles_per_second;
			buffer_in_progress_.resize((size_t)buffer_size);
			set_needs_updated_filter_coefficients();
		}

		void set_output_quality(int number_of_taps) {
			requested_number_of_taps_ = (size_t)number_of_taps;
			set_needs_updated_filter_coefficients();
		}

		void set_delegate(Delegate *delegate) {
			delegate_ = delegate;
		}

		void set_input_rate(float cycles_per_second) {
			input_cycles_per_second_ = cycles_per_second;
			set_needs_updated_filter_coefficients();
		}

		/*!
			Sets the cut-off frequency for a low-pass filter attached to the output of this speaker; optional.
		*/
		void set_high_frequency_cut_off(float high_frequency) {
			high_frequency_cut_off_ = high_frequency;
			set_needs_updated_filter_coefficients();
		}

		Speaker() : buffer_in_progress_pointer_(0), requested_number_of_taps_(0), high_frequency_cut_off_(-1.0), _queue(new Concurrency::AsyncTaskQueue) {}

		/*!
			Ensures any deferred processing occurs now.
		*/
		void flush() {
			std::shared_ptr<std::list<std::function<void(void)>>> queued_functions = queued_functions_;
			queued_functions_.reset();
			_queue->enqueue([queued_functions] {
				for(auto function : *queued_functions) {
					function();
				}
			});
		}

	protected:
		void enqueue(std::function<void(void)> function) {
			if(!queued_functions_) queued_functions_.reset(new std::list<std::function<void(void)>>);
			queued_functions_->push_back(function);
		}
		std::shared_ptr<std::list<std::function<void(void)>>> queued_functions_;

		std::vector<int16_t> buffer_in_progress_;
		float high_frequency_cut_off_;
		size_t buffer_in_progress_pointer_;
		size_t number_of_taps_, requested_number_of_taps_;
		bool coefficients_are_dirty_;
		Delegate *delegate_;

		float input_cycles_per_second_, output_cycles_per_second_;

		void set_needs_updated_filter_coefficients() {
			coefficients_are_dirty_ = true;
		}

		void get_samples(unsigned int quantity, int16_t *target)	{}
		void skip_samples(unsigned int quantity) {
			int16_t throwaway_samples[quantity];
			get_samples(quantity, throwaway_samples);
		}

		std::unique_ptr<Concurrency::AsyncTaskQueue> _queue;
};

/*!
	A concrete descendant of Speaker that uses a FIR filter to map from input data to output data when scaling
	and a copy-through buffer when input and output rates are the same.

	Audio sources should use @c Filter as both a template and a parent, implementing at least
	`get_samples(unsigned int quantity, int16_t *target)` and ideally also `skip_samples(unsigned int quantity)`
	to provide source data.

	Call `run_for_cycles(n)` to request that the next n cycles of input data are collected.
*/
template <class T> class Filter: public Speaker {
	public:
		~Filter() {
			_queue->flush();
		}

		void run_for_cycles(unsigned int input_cycles) {
			enqueue([=]() {
				unsigned int cycles_remaining = input_cycles;
				if(coefficients_are_dirty_) update_filter_coefficients();

				// if input and output rates exactly match, just accumulate results and pass on
				if(input_cycles_per_second_ == output_cycles_per_second_ && high_frequency_cut_off_ < 0.0) {
					while(cycles_remaining) {
						unsigned int cycles_to_read = (unsigned int)(buffer_in_progress_.size() - (size_t)buffer_in_progress_pointer_);
						if(cycles_to_read > cycles_remaining) cycles_to_read = cycles_remaining;

						static_cast<T *>(this)->get_samples(cycles_to_read, &buffer_in_progress_[(size_t)buffer_in_progress_pointer_]);
						buffer_in_progress_pointer_ += cycles_to_read;

						// announce to delegate if full
						if(buffer_in_progress_pointer_ == buffer_in_progress_.size()) {
							buffer_in_progress_pointer_ = 0;
							if(delegate_) {
								delegate_->speaker_did_complete_samples(this, buffer_in_progress_);
							}
						}

						cycles_remaining -= cycles_to_read;
					}

					return;
				}

				// if the output rate is less than the input rate, use the filter
				if(input_cycles_per_second_ > output_cycles_per_second_ || (input_cycles_per_second_ == output_cycles_per_second_ && high_frequency_cut_off_ >= 0.0)) {
					while(cycles_remaining) {
						unsigned int cycles_to_read = (unsigned int)std::min((size_t)cycles_remaining, number_of_taps_ - input_buffer_depth_);
						static_cast<T *>(this)->get_samples(cycles_to_read, &input_buffer_[(size_t)input_buffer_depth_]);
						cycles_remaining -= cycles_to_read;
						input_buffer_depth_ += cycles_to_read;

						if(input_buffer_depth_ == number_of_taps_) {
							buffer_in_progress_[(size_t)buffer_in_progress_pointer_] = filter_->apply(input_buffer_.data());
							buffer_in_progress_pointer_++;

							// announce to delegate if full
							if(buffer_in_progress_pointer_ == buffer_in_progress_.size()) {
								buffer_in_progress_pointer_ = 0;
								if(delegate_) {
									delegate_->speaker_did_complete_samples(this, buffer_in_progress_);
								}
							}

							// If the next loop around is going to reuse some of the samples just collected, use a memmove to
							// preserve them in the correct locations (TODO: use a longer buffer to fix that) and don't skip
							// anything. Otherwise skip as required to get to the next sample batch and don't expect to reuse.
							uint64_t steps = stepper_->step();
							if(steps < number_of_taps_) {
								int16_t *input_buffer = input_buffer_.data();
								memmove(input_buffer, &input_buffer[steps], sizeof(int16_t) * ((size_t)number_of_taps_ - (size_t)steps));
								input_buffer_depth_ -= steps;
							} else {
								if(steps > number_of_taps_)
									static_cast<T *>(this)->skip_samples((unsigned int)steps - (unsigned int)number_of_taps_);
								input_buffer_depth_ = 0;
							}
						}
					}

					return;
				}

			// TODO: input rate is less than output rate
			});
		}

	private:
		std::unique_ptr<SignalProcessing::Stepper> stepper_;
		std::unique_ptr<SignalProcessing::FIRFilter> filter_;

		std::vector<int16_t> input_buffer_;
		size_t input_buffer_depth_;

		void update_filter_coefficients() {
			// make a guess at a good number of taps if this hasn't been provided explicitly
			if(requested_number_of_taps_) {
				number_of_taps_ = requested_number_of_taps_;
			} else {
				number_of_taps_ = (size_t)ceilf((input_cycles_per_second_ + output_cycles_per_second_) / output_cycles_per_second_);
				number_of_taps_ *= 2;
				number_of_taps_ |= 1;
			}

			coefficients_are_dirty_ = false;
			buffer_in_progress_pointer_ = 0;

			stepper_.reset(new SignalProcessing::Stepper((uint64_t)input_cycles_per_second_, (uint64_t)output_cycles_per_second_));

			float high_pass_frequency;
			if(high_frequency_cut_off_ > 0.0) {
				high_pass_frequency = std::min(output_cycles_per_second_ / 2.0f, high_frequency_cut_off_);
			} else {
				high_pass_frequency = output_cycles_per_second_ / 2.0f;
			}
			filter_.reset(new SignalProcessing::FIRFilter((unsigned int)number_of_taps_, (float)input_cycles_per_second_, 0.0, high_pass_frequency, SignalProcessing::FIRFilter::DefaultAttenuation));

			input_buffer_.resize((size_t)number_of_taps_);
			input_buffer_depth_ = 0;
		}
};

}

#endif /* Speaker_hpp */
