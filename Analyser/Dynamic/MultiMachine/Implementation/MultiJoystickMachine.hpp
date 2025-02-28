//
//  MultiJoystickMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Machines/DynamicMachine.hpp"

#include <memory>
#include <vector>

namespace Analyser::Dynamic {

/*!
	Provides a class that multiplexes the joystick machine interface to multiple machines.

	Makes a static internal copy of the list of machines; makes no guarantees about the
	order of delivered messages.
*/
class MultiJoystickMachine: public MachineTypes::JoystickMachine {
public:
	MultiJoystickMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &);

	// Below is the standard JoystickMachine::Machine interface; see there for documentation.
	const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() final;

private:
	std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;
};

}
