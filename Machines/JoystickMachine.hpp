//
//  JoystickMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/10/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Inputs/Joystick.hpp"
#include <vector>

namespace MachineTypes {

struct JoystickMachine {
	virtual const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() = 0;
};

}
