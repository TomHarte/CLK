//
//  Mouse.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

namespace Inputs {

/*!
	Models a classic-era mouse: something that provides 2d relative motion plus
	some quantity of buttons.
*/
class Mouse {
	public:
		/*!
			Indicates a movement of the mouse.
		*/
		virtual void move([[maybe_unused]] const int x, [[maybe_unused]] const int y) {}

		/*!
			@returns the number of buttons on this mouse.
		*/
		virtual int get_number_of_buttons() const {
			return 1;
		}

		/*!
			Indicates that button @c index is now either pressed or unpressed.
			The intention is that @c index be semantic, not positional:
			0 for the primary button, 1 for the secondary, 2 for the tertiary, etc.
		*/
		virtual void set_button_pressed([[maybe_unused]] const int index, [[maybe_unused]] const bool is_pressed) {}

		/*!
			Releases all depressed buttons.
		*/
		virtual void reset_all_buttons() {}
};

}
