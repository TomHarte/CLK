//
//  Joystick.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Joystick_hpp
#define Joystick_hpp

#include <vector>

namespace Inputs {

/*!
	Provides an intermediate idealised model of a simple joystick, allowing a host
	machine to toggle states, while an interested party either observes or polls.
*/
class Joystick {
	public:
		virtual ~Joystick() {}
	
		enum class DigitalInput {
			Up, Down, Left, Right, Fire
		};

		// Host interface.
		virtual void set_digital_input(DigitalInput digital_input, bool is_active) = 0;
		virtual void reset_all_inputs() = 0;
};

}

#endif /* Joystick_hpp */
