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
#include "../SignalProcessing/Stepper.hpp"

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
				delete[] _buffer_in_progress;
				_buffer_in_progress = new int16_t[buffer_size];
				_buffer_size = buffer_size;
			}
			set_needs_updated_filter_coefficients();
		}

		void set_output_quality(int number_of_taps)
		{
			_number_of_taps = number_of_taps;
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

		Speaker() : _buffer_in_progress_pointer(0) {}

	protected:
		int16_t *_buffer_in_progress;
		int _buffer_size;
		int _buffer_in_progress_pointer;
		int _number_of_taps;
		bool _coefficients_are_dirty;
		Delegate *_delegate;
		SignalProcessing::Stepper *_stepper;

		int _input_cycles_per_second, _output_cycles_per_second;

		void set_needs_updated_filter_coefficients()
		{
			_coefficients_are_dirty = true;
		}
};

template <class T> class Filter: public Speaker {
	public:
		void run_for_cycles(int input_cycles)
		{
			if(_coefficients_are_dirty) update_filter_coefficients();

			// point sample for now, as a temporary measure
			while(input_cycles--)
			{
				// get a sample for the current location
				static_cast<T *>(this)->get_samples(1, &_buffer_in_progress[_buffer_in_progress_pointer]);
				_buffer_in_progress_pointer++;

				// announce to delegate if full
				if(_buffer_in_progress_pointer == _buffer_size)
				{
					_buffer_in_progress_pointer = 0;
					if(_delegate)
					{
						_delegate->speaker_did_complete_samples(this, _buffer_in_progress, _buffer_size);
					}
				}

				// determine how many source samples to step
				uint64_t steps = _stepper->update();
				if(steps > 1)
					static_cast<T *>(this)->skip_samples((unsigned int)(steps-1));
			}
		}

	private:
		SignalProcessing::Stepper *_stepper;

		void update_filter_coefficients()
		{
			_coefficients_are_dirty = false;
			_buffer_in_progress_pointer = 0;

			delete _stepper;
			_stepper = new SignalProcessing::Stepper((uint64_t)_input_cycles_per_second, (uint64_t)_output_cycles_per_second);
		}
};

}

#endif /* Speaker_hpp */
