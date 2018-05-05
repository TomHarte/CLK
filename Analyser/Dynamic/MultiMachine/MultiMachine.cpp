//
//  MultiMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MultiMachine.hpp"

#include <algorithm>

using namespace Analyser::Dynamic;

MultiMachine::MultiMachine(std::vector<std::unique_ptr<DynamicMachine>> &&machines) :
	machines_(std::move(machines)),
	configurable_(machines_),
	configuration_target_(machines_),
	crt_machine_(machines_, machines_mutex_),
	joystick_machine_(machines),
	keyboard_machine_(machines_) {
	crt_machine_.set_delegate(this);
}

ConfigurationTarget::Machine *MultiMachine::configuration_target() {
	if(has_picked_) {
		return machines_.front()->configuration_target();
	} else {
		return &configuration_target_;
	}
}

CRTMachine::Machine *MultiMachine::crt_machine() {
	if(has_picked_) {
		return machines_.front()->crt_machine();
	} else {
		return &crt_machine_;
	}
}

JoystickMachine::Machine *MultiMachine::joystick_machine() {
	if(has_picked_) {
		return machines_.front()->joystick_machine();
	} else {
		return &joystick_machine_;
	}
}

KeyboardMachine::Machine *MultiMachine::keyboard_machine() {
	if(has_picked_) {
		return machines_.front()->keyboard_machine();
	} else {
		return &keyboard_machine_;
	}
}

Configurable::Device *MultiMachine::configurable_device() {
	if(has_picked_) {
		return machines_.front()->configurable_device();
	} else {
		return &configurable_;
	}
}

bool MultiMachine::would_collapse(const std::vector<std::unique_ptr<DynamicMachine>> &machines) {
	return
		(machines.front()->crt_machine()->get_confidence() > 0.9f) ||
		(machines.front()->crt_machine()->get_confidence() >= 2.0f * machines[1]->crt_machine()->get_confidence());
}

void MultiMachine::multi_crt_did_run_machines() {
	std::lock_guard<std::mutex> machines_lock(machines_mutex_);
#ifdef DEBUG
	for(const auto &machine: machines_) {
		CRTMachine::Machine *crt = machine->crt_machine();
		printf("%0.2f ", crt->get_confidence());
		crt->print_type();
		printf("; ");
	}
	printf("\n");
#endif

	DynamicMachine *front = machines_.front().get();
	std::stable_sort(machines_.begin(), machines_.end(),
		[] (const std::unique_ptr<DynamicMachine> &lhs, const std::unique_ptr<DynamicMachine> &rhs){
			CRTMachine::Machine *lhs_crt = lhs->crt_machine();
			CRTMachine::Machine *rhs_crt = rhs->crt_machine();
			return lhs_crt->get_confidence() > rhs_crt->get_confidence();
		});

	if(machines_.front().get() != front) {
		crt_machine_.did_change_machine_order();
	}

	if(would_collapse(machines_)) {
		pick_first();
	}
}

void MultiMachine::pick_first() {
	has_picked_ = true;
//	machines_.erase(machines_.begin() + 1, machines_.end());
	// TODO: this isn't quite correct, because it may leak OpenGL/etc resources through failure to
	// request a close_output while the context is active.
}

void *MultiMachine::raw_pointer() {
	return nullptr;
}
