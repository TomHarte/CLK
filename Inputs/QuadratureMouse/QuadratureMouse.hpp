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
			Gets and removes a single step from the current accumulated mouse
			movement for @c axis — axis 0 is x, axis 1 is y.

			@returns 0 if no movement is outstanding, -1 if there is outstanding
			negative movement, +1 if there is outstanding positive movement.
		*/
		int get_step(int axis) {
			if(!axes_[axis]) return 0;

			if(axes_[axis] > 0) {
				-- axes_[axis];
				return 1;
			} else {
				++ axes_[axis];
				return -1;
			}
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

};

}

#endif /* QuadratureMouse_hpp */
