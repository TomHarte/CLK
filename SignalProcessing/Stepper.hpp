//
//  Stepper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Stepper_hpp
#define Stepper_hpp

#include <cstdint>

namespace SignalProcessing {

/*!
	Allows a repeating action running at an input rate to determine how many times it should
	trigger an action that runs at an unrelated output rate; therefore it allows something with one
	clock to sample something with another.

	Uses a Bresenham-like error term internally for full-integral storage with no drift.

	Pegs the beginning of both clocks to the time at which the stepper is created. So e.g. a stepper
	that converts from an input clock of 1200 to an output clock of 2 will first fire on cycle 600.
*/
class Stepper {
	public:
		/*!
			Establishes a stepper with a one-to-one conversion rate.
		*/
		Stepper() : Stepper(1,1) {}

		/*!
			Establishes a stepper that will receive steps at the @c input_rate and dictate the number
			of steps that should be taken at the @c output_rate.
		*/
		Stepper(uint64_t output_rate, uint64_t input_rate) :
			accumulated_error_(-(int64_t(input_rate) << 1)),
			input_rate_(input_rate),
			output_rate_(output_rate),
			whole_step_(output_rate / input_rate),
			adjustment_up_(int64_t(output_rate % input_rate) << 1),
			adjustment_down_(int64_t(input_rate) << 1) {}

		/*!
			Advances one step at the input rate.

			@returns the number of output steps.
		*/
		inline uint64_t step() {
			uint64_t update = whole_step_;
			accumulated_error_ += adjustment_up_;
			if(accumulated_error_ > 0) {
				update++;
				accumulated_error_ -= adjustment_down_;
			}
			return update;
		}

		/*!
			Advances by @c number_of_steps steps at the input rate.

			@returns the number of output steps.
		*/
		inline uint64_t step(uint64_t number_of_steps) {
			uint64_t update = whole_step_ * number_of_steps;
			accumulated_error_ += adjustment_up_ * int64_t(number_of_steps);
			if(accumulated_error_ > 0) {
				update += 1 + uint64_t(accumulated_error_ / adjustment_down_);
				accumulated_error_ = (accumulated_error_ % adjustment_down_) - adjustment_down_;
			}
			return update;
		}

		/*!
			@returns the output rate.
		*/
		inline uint64_t get_output_rate() {
			return output_rate_;
		}

		/*!
			@returns the input rate.
		*/
		inline uint64_t get_input_rate() {
			return input_rate_;
		}

	private:
		int64_t accumulated_error_;
		uint64_t input_rate_, output_rate_;
		uint64_t whole_step_;
		int64_t adjustment_up_, adjustment_down_;
};

}

#endif /* Stepper_hpp */
