//
//  MultiMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MultiMachine.hpp"

using namespace Analyser::Dynamic;

MultiMachine::MultiMachine(std::vector<std::unique_ptr<DynamicMachine>> &&machines) :
	machines_(std::move(machines)),
	configuration_target_(machines_) {}

ConfigurationTarget::Machine *MultiMachine::configuration_target() {
	return &configuration_target_;
}

CRTMachine::Machine *MultiMachine::crt_machine() {
	return nullptr;
}

JoystickMachine::Machine *MultiMachine::joystick_machine() {
	return nullptr;
}

KeyboardMachine::Machine *MultiMachine::keyboard_machine() {
	return nullptr;
}

Configurable::Device *MultiMachine::configurable_device() {
	return nullptr;
}

Utility::TypeRecipient *MultiMachine::type_recipient() {
	return nullptr;
}
