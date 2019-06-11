//
//  QuadratureMouse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef QuadratureMouse_hpp
#define QuadratureMouse_hpp

#include "../Mouse.hpp"
#include <atomic>

namespace Inputs {

/*!
	Provides a simple implementation of a Mouse, designed for simple
	thread-safe feeding to a machine that accepts quadrature-encoded input.
*/
class QuadratureMouse: public Mouse {
	public:
		QuadratureMouse(int number_of_buttons) :
			number_of_buttons_(number_of_buttons) {}

		/*
			Inputs, to satisfy the Mouse interface.
		*/
		void move(int x, int y) override {
			// Accumulate all provided motion.
			axes_[0] += x;
			axes_[1] += y;
		}

		int get_number_of_buttons() override {
			return number_of_buttons_;
		}

		void set_button_pressed(int index, bool is_pressed) override {
			if(is_pressed)
				button_flags_ |= (1 << index);
			else
				button_flags_ &= ~(1 << index);
		}

		void reset_all_buttons() override {
			button_flags_ = 0;
		}

		/*
			Outputs.
		*/

		/*!
			Removes a single step from the current accumulated mouse movement;
			the step removed will henceforth be queriable via get_step.
		*/
		void prepare_step() {
			for(int axis = 0; axis < 2; ++axis) {
				const int axis_value = axes_[axis];
				if(!axis_value) {
					step_[axis] = 0;
				} else {
					if(axis_value > 0) {
						-- axes_[axis];
						step_[axis] = 1;
					} else {
						++ axes_[axis];
						step_[axis] = -1;
					}
				}
			}
		}

		/*!
			Gets and removes a single step from the current accumulated mouse
			movement for @c axis — axis 0 is x, axis 1 is y.

			@returns 0 if no movement is outstanding, -1 if there is outstanding
			negative movement, +1 if there is outstanding positive movement.
		*/
		int get_step(int axis) {
			return step_[axis];
		}

		/*!
			@returns a bit mask of the currently pressed buttons.
		*/
		int get_button_mask() {
			return button_flags_;
		}

	private:
		int number_of_buttons_ = 0;
		std::atomic<int> button_flags_;
		std::atomic<int> axes_[2];
		int step_[2];

};

}

#endif /* QuadratureMouse_hpp */
