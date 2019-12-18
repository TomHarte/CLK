//
//  JoystickMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef JoystickMachine_hpp
#define JoystickMachine_hpp

#include "../Inputs/Joystick.hpp"
#include <vector>

namespace JoystickMachine {

class Machine {
	public:
		virtual const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() = 0;
};

}

#endif /* JoystickMachine_hpp */
