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
	configuration_target_(machines_),
	crt_machine_(machines_) {
	crt_machine_.set_delegate(this);
}

ConfigurationTarget::Machine *MultiMachine::configuration_target() {
	return &configuration_target_;
}

CRTMachine::Machine *MultiMachine::crt_machine() {
	return &crt_machine_;
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

void MultiMachine::multi_crt_did_run_machines() {
	DynamicMachine *front = machines_.front().get();
//	for(const auto &machine: machines_) {
//		CRTMachine::Machine *crt = machine->crt_machine();
//		printf("%0.2f ", crt->get_confidence());
//		crt->print_type();
//		printf("; ");
//	}
//	printf("\n");
	std::stable_sort(machines_.begin(), machines_.end(), [] (const auto &lhs, const auto &rhs){
		CRTMachine::Machine *lhs_crt = lhs->crt_machine();
		CRTMachine::Machine *rhs_crt = rhs->crt_machine();
		return lhs_crt->get_confidence() > rhs_crt->get_confidence();
	});

	if(machines_.front().get() != front) {
		crt_machine_.did_change_machine_order();
	}
}
