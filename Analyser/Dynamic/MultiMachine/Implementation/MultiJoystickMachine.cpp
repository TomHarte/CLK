//
//  MultiJoystickMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/02/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MultiJoystickMachine.hpp"

#include <algorithm>

using namespace Analyser::Dynamic;

namespace {

class MultiJoystick: public Inputs::Joystick {
public:
	MultiJoystick(std::vector<MachineTypes::JoystickMachine *> &machines, const std::size_t index) {
		for(const auto &machine: machines) {
			const auto &joysticks = machine->get_joysticks();
			if(joysticks.size() > index) {
				joysticks_.push_back(joysticks[index].get());
			}
		}
	}

	const std::vector<Input> &get_inputs() final {
		if(inputs.empty()) {
			for(const auto &joystick: joysticks_) {
				std::vector<Input> joystick_inputs = joystick->get_inputs();
				for(const auto &input: joystick_inputs) {
					if(std::find(inputs.begin(), inputs.end(), input) != inputs.end()) {
						inputs.push_back(input);
					}
				}
			}
		}

		return inputs;
	}

	void set_input(const Input &digital_input, const bool is_active) final {
		for(const auto &joystick: joysticks_) {
			joystick->set_input(digital_input, is_active);
		}
	}

	void set_input(const Input &digital_input, const float value) final {
		for(const auto &joystick: joysticks_) {
			joystick->set_input(digital_input, value);
		}
	}

	void reset_all_inputs() final {
		for(const auto &joystick: joysticks_) {
			joystick->reset_all_inputs();
		}
	}

private:
	std::vector<Input> inputs;
	std::vector<Inputs::Joystick *> joysticks_;
};

}

MultiJoystickMachine::MultiJoystickMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) {
	std::size_t total_joysticks = 0;
	std::vector<MachineTypes::JoystickMachine *> joystick_machines;
	for(const auto &machine: machines) {
		auto joystick_machine = machine->joystick_machine();
		if(joystick_machine) {
			joystick_machines.push_back(joystick_machine);
			total_joysticks = std::max(total_joysticks, joystick_machine->get_joysticks().size());
		}
	}

	for(std::size_t index = 0; index < total_joysticks; ++index) {
		joysticks_.emplace_back(new MultiJoystick(joystick_machines, index));
	}
}

const std::vector<std::unique_ptr<Inputs::Joystick>> &MultiJoystickMachine::get_joysticks() {
	return joysticks_;
}
