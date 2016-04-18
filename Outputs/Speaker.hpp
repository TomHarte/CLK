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

namespace Outputs {

class Speaker {
	public:
		class Delegate {
			public:
				virtual void speaker_did_complete_samples(Speaker *speaker, const int16_t *buffer, int buffer_size) = 0;
		};

		void set_output_rate(int cycles_per_second, int buffer_size)
		{
			_output_cycles_per_second = cycles_per_second;
			if(_buffer_size != buffer_size)
			{
				_buffer_in_progress = std::unique_ptr<int16_t>(new int16_t[buffer_size]);
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

		void set_input_rate(int cycles_per_second)
		{
			_input_cycles_per_second = cycles_per_second;
			set_needs_updated_filter_coefficients();
		}

		Speaker() : _buffer_in_progress_pointer(0), _requested_number_of_taps(0) {}

	protected:
		std::unique_ptr<int16_t> _buffer_in_progress;
		int _buffer_size;
		int _buffer_in_progress_pointer;
		int _number_of_taps, _requested_number_of_taps;
		bool _coefficients_are_dirty;
		Delegate *_delegate;

		int _input_cycles_per_second, _output_cycles_per_second;

		void set_needs_updated_filter_coefficients()
		{
			_coefficients_are_dirty = true;
		}
};

template <class T> class Filter: public Speaker {
	public:
		void run_for_cycles(unsigned int input_cycles)
		{
			if(_coefficients_are_dirty) update_filter_coefficients();

			// TODO: what if output rate is greater than input rate?

			// fill up as much of the input buffer as possible
			while(input_cycles)
			{
				unsigned int cycles_to_read = (unsigned int)std::min((int)input_cycles, _number_of_taps - _input_buffer_depth);
				static_cast<T *>(this)->get_samples(cycles_to_read, &_input_buffer.get()[_input_buffer_depth]);
				input_cycles -= cycles_to_read;
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
						memmove(input_buffer, &input_buffer[steps], sizeof(int16_t)  * ((size_t)_number_of_taps - (size_t)steps));
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
				_number_of_taps = (_input_cycles_per_second + _output_cycles_per_second) / _output_cycles_per_second;
				_number_of_taps *= 2;
				_number_of_taps |= 1;
			}

			_coefficients_are_dirty = false;
			_buffer_in_progress_pointer = 0;

			_stepper = std::unique_ptr<SignalProcessing::Stepper>(new SignalProcessing::Stepper((uint64_t)_input_cycles_per_second, (uint64_t)_output_cycles_per_second));
			_filter = std::unique_ptr<SignalProcessing::FIRFilter>(new SignalProcessing::FIRFilter((unsigned int)_number_of_taps, (unsigned int)_input_cycles_per_second, 0.0, (float)_output_cycles_per_second / 2.0f, SignalProcessing::FIRFilter::DefaultAttenuation));

			_input_buffer = std::unique_ptr<int16_t>(new int16_t[_number_of_taps]);
			_input_buffer_depth = 0;
		}
};

}

#endif /* Speaker_hpp */
