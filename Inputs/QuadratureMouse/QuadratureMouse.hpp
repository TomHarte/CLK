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

	TEMPORARY SIMPLIFICATION: it is assumed that the caller will be interested
	in observing a signal that dictates velocity, sampling the other to
	obtain direction only on transitions in the velocity signal.

	Or, more concretely, of the two channels per axis, one is accurate only when
	the other transitions. Hence the discussion of 'primary' and 'secondary'
	channels below. This is intended to be fixed.
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
			Applies a single step from the current accumulated mouse movement, which
			might involve the mouse moving right, or left, or not at all.
		*/
		void prepare_step() {
			for(int axis = 0; axis < 2; ++axis) {
				// Do nothing if there's no motion to communicate.
				const int axis_value = axes_[axis];
				if(!axis_value) continue;

				// Toggle the primary channel and set the secondary for
				// negative motion. At present the y axis signals the
				// secondary channel the opposite way around from the
				// primary.
				primaries_[axis] ^= 1;
				secondaries_[axis] = primaries_[axis] ^ axis;
				if(axis_value > 0) {
					-- axes_[axis];
					secondaries_[axis] ^= 1;	// Switch to positive motion.
				} else {
					++ axes_[axis];
				}
			}
		}

		/*!
			@returns the two quadrature channels — bit 0 is the 'primary' channel
				(i.e. the one that can be monitored to observe velocity) and
				bit 1 is the 'secondary' (i.e. that which can be queried to
				observe direction).
		*/
		int get_channel(int axis) {
			return primaries_[axis] | (secondaries_[axis] << 1);
		}

		/*!
			@returns a bit mask of the currently pressed buttons.
		*/
		int get_button_mask() {
			return button_flags_;
		}

		/*!
			@returns @c true if any mouse motion is waiting to be communicated;
				@c false otherwise.
		*/
		bool has_steps() {
			return axes_[0] || axes_[1];
		}

	private:
		int number_of_buttons_ = 0;
		std::atomic<int> button_flags_;
		std::atomic<int> axes_[2];

		int primaries_[2] = {0, 0};
		int secondaries_[2] = {0, 0};
};

}

#endif /* QuadratureMouse_hpp */
