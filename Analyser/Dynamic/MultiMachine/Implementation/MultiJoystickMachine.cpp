//
//  MultiJoystickMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MultiJoystickMachine.hpp"

using namespace Analyser::Dynamic;

MultiJoystickMachine::MultiJoystickMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	for(const auto &machine: machines) {
		JoystickMachine::Machine *joystick_machine = machine->joystick_machine();
		if(joystick_machine) machines_.push_back(joystick_machine);
    }
}

std::vector<std::unique_ptr<Inputs::Joystick>> &MultiJoystickMachine::get_joysticks() {
	return joysticks_;
}
