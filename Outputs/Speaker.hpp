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
class Speaker: public Concurrency::AsyncTaskQueue {
	public:
		class Delegate {
			public:
				virtual void speaker_did_complete_samples(Speaker *speaker, const int16_t *buffer, int buffer_size) = 0;
		};

		float get_ideal_clock_rate_in_range(float minimum, float maximum)
		{
			// return twice the cut off, if applicable
			if(_high_frequency_cut_off > 0.0f && _input_cycles_per_second >= _high_frequency_cut_off * 3.0f && _input_cycles_per_second <= _high_frequency_cut_off * 3.0f) return _high_frequency_cut_off * 3.0f;

			// return exactly the input rate if possible
			if(_input_cycles_per_second >= minimum && _input_cycles_per_second <= maximum) return _input_cycles_per_second;

			// if the input rate is lower, return the minimum
			if(_input_cycles_per_second < minimum) return minimum;

			// otherwise, return the maximum
			return maximum;
		}

		void set_output_rate(float cycles_per_second, int buffer_size)
		{
			_output_cycles_per_second = cycles_per_second;
			if(_buffer_size != buffer_size)
			{
				_buffer_in_progress.reset(new int16_t[buffer_size]);
				_buffer_size = buffer_size;
			}
			set_needs_updated_filter_coefficients();
		}

		void set_output_quality(int number_of_taps)
		{
			_requested_number_of_taps = number_of_taps;
			set_needs_updated_filter_coefficients();
		}

		void set_delegate(Delegate *delegate)
		{
			_delegate = delegate;
		}

		void set_input_rate(float cycles_per_second)
		{
			_input_cycles_per_second = cycles_per_second;
			set_needs_updated_filter_coefficients();
		}

		/*!
			Sets the cut-off frequency for a low-pass filter attached to the output of this speaker; optional.
		*/
		void set_high_frequency_cut_off(float high_frequency)
		{
			_high_frequency_cut_off = high_frequency;
			set_needs_updated_filter_coefficients();
		}

		Speaker() : _buffer_in_progress_pointer(0), _requested_number_of_taps(0), _high_frequency_cut_off(-1.0) {}

	protected:
		std::unique_ptr<int16_t> _buffer_in_progress;
		float _high_frequency_cut_off;
		int _buffer_size;
		int _buffer_in_progress_pointer;
		int _number_of_taps, _requested_number_of_taps;
		bool _coefficients_are_dirty;
		Delegate *_delegate;

		float _input_cycles_per_second, _output_cycles_per_second;

		void set_needs_updated_filter_coefficients()
		{
			_coefficients_are_dirty = true;
		}

		void get_samples(unsigned int quantity, int16_t *target)	{}
		void skip_samples(unsigned int quantity)
		{
			int16_t throwaway_samples[quantity];
			get_samples(quantity, throwaway_samples);
		}
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
		void run_for_cycles(unsigned int input_cycles)
		{
			enqueue([=]() {
				unsigned int cycles_remaining = input_cycles;
				if(_coefficients_are_dirty) update_filter_coefficients();

				// if input and output rates exactly match, just accumulate results and pass on
				if(_input_cycles_per_second == _output_cycles_per_second && _high_frequency_cut_off < 0.0)
				{
					while(cycles_remaining)
					{
						unsigned int cycles_to_read = (unsigned int)(_buffer_size - _buffer_in_progress_pointer);
						if(cycles_to_read > cycles_remaining) cycles_to_read = cycles_remaining;

						static_cast<T *>(this)->get_samples(cycles_to_read, &_buffer_in_progress.get()[_buffer_in_progress_pointer]);
						_buffer_in_progress_pointer += cycles_to_read;

						// announce to delegate if full
						if(_buffer_in_progress_pointer == _buffer_size)
						{
							_buffer_in_progress_pointer = 0;
							if(_delegate)
							{
								_delegate->speaker_did_complete_samples(this, _buffer_in_progress.get(), _buffer_size);
							}
						}

						cycles_remaining -= cycles_to_read;
					}

					return;
				}

				// if the output rate is less than the input rate, use the filter
				if(_input_cycles_per_second > _output_cycles_per_second)
				{
					while(cycles_remaining)
					{
						unsigned int cycles_to_read = (unsigned int)std::min((int)cycles_remaining, _number_of_taps - _input_buffer_depth);
						static_cast<T *>(this)->get_samples(cycles_to_read, &_input_buffer.get()[_input_buffer_depth]);
						cycles_remaining -= cycles_to_read;
						_input_buffer_depth += cycles_to_read;

						if(_input_buffer_depth == _number_of_taps)
						{
							_buffer_in_progress.get()[_buffer_in_progress_pointer] = _filter->apply(_input_buffer.get());
							_buffer_in_progress_pointer++;

							// announce to delegate if full
							if(_buffer_in_progress_pointer == _buffer_size)
							{
								_buffer_in_progress_pointer = 0;
								if(_delegate)
								{
									_delegate->speaker_did_complete_samples(this, _buffer_in_progress.get(), _buffer_size);
								}
							}

							// If the next loop around is going to reuse some of the samples just collected, use a memmove to
							// preserve them in the correct locations (TODO: use a longer buffer to fix that) and don't skip
							// anything. Otherwise skip as required to get to the next sample batch and don't expect to reuse.
							uint64_t steps = _stepper->step();
							if(steps < _number_of_taps)
							{
								int16_t *input_buffer = _input_buffer.get();
								memmove(input_buffer, &input_buffer[steps], sizeof(int16_t) * ((size_t)_number_of_taps - (size_t)steps));
								_input_buffer_depth -= steps;
							}
							else
							{
								if(steps > _number_of_taps)
									static_cast<T *>(this)->skip_samples((unsigned int)steps - (unsigned int)_number_of_taps);
								_input_buffer_depth = 0;
							}
						}
					}

					return;
				}

			// TODO: input rate is less than output rate
			});
		}

	private:
		std::unique_ptr<SignalProcessing::Stepper> _stepper;
		std::unique_ptr<SignalProcessing::FIRFilter> _filter;

		std::unique_ptr<int16_t> _input_buffer;
		int _input_buffer_depth;

		void update_filter_coefficients()
		{
			// make a guess at a good number of taps if this hasn't been provided explicitly
			if(_requested_number_of_taps)
			{
				_number_of_taps = _requested_number_of_taps;
			}
			else
			{
				_number_of_taps = (int)ceilf((_input_cycles_per_second + _output_cycles_per_second) / _output_cycles_per_second);
				_number_of_taps *= 2;
				_number_of_taps |= 1;
			}

			_coefficients_are_dirty = false;
			_buffer_in_progress_pointer = 0;

			_stepper.reset(new SignalProcessing::Stepper((uint64_t)_input_cycles_per_second, (uint64_t)_output_cycles_per_second));

			float high_pass_frequency;
			if(_high_frequency_cut_off > 0.0)
			{
				high_pass_frequency = std::min((float)_output_cycles_per_second / 2.0f, _high_frequency_cut_off);
			}
			else
			{
				high_pass_frequency = (float)_output_cycles_per_second / 2.0f;
			}
			_filter.reset(new SignalProcessing::FIRFilter((unsigned int)_number_of_taps, (float)_input_cycles_per_second, 0.0, high_pass_frequency, SignalProcessing::FIRFilter::DefaultAttenuation));

			_input_buffer.reset(new int16_t[_number_of_taps]);
			_input_buffer_depth = 0;
		}
};

}

#endif /* Speaker_hpp */
