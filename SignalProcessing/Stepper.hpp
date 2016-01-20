//
//  Stepper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Stepper_hpp
#define Stepper_hpp

#include <stdint.h>

namespace SignalProcessing {

class Stepper
{
	public:
		Stepper()
		{
			Stepper(1, 1);
		}

		Stepper(uint64_t output_rate, uint64_t input_rate)
		{
			input_rate_ = input_rate;
			output_rate_ = output_rate;
			whole_step_ = output_rate / input_rate;
			adjustment_up_ = (int64_t)(output_rate % input_rate) << 1;
			adjustment_down_ = (int64_t)input_rate << 1;
		}

		inline uint64_t step()
		{
			uint64_t update = whole_step_;
			accumulated_error_ += adjustment_up_;
			if(accumulated_error_ > 0)
			{
				update++;
				accumulated_error_ -= adjustment_down_;
			}
			return update;
		}

		inline uint64_t get_output_rate()
		{
			return output_rate_;
		}

		inline uint64_t get_input_rate()
		{
			return input_rate_;
		}

	private:
		uint64_t whole_step_;
		int64_t adjustment_up_, adjustment_down_;
		int64_t accumulated_error_;
		uint64_t input_rate_, output_rate_;
};

}

#endif /* Stepper_hpp */
