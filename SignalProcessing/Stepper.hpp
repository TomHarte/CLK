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
		Stepper(uint64_t output_rate, uint64_t update_rate)
		{
			whole_step_ = output_rate / update_rate;
			adjustment_up_ = (int64_t)(output_rate % update_rate) << 1;
			adjustment_down_ = (int64_t)update_rate << 1;
		}

		inline uint64_t update()
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

	private:
		uint64_t whole_step_;
		int64_t adjustment_up_, adjustment_down_;
		int64_t accumulated_error_;
};

}

#endif /* Stepper_hpp */
