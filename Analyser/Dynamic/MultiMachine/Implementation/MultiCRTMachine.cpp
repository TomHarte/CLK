//
//  MultiCRTMachine.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MultiCRTMachine.hpp"

using namespace Analyser::Dynamic;

MultiCRTMachine::MultiCRTMachine(const std::vector<std::unique_ptr<::Machine::DynamicMachine>> &machines) :
	machines_(machines) {}

void MultiCRTMachine::setup_output(float aspect_ratio) {
	for(const auto &machine: machines_) {
		CRTMachine::Machine *crt_machine = machine->crt_machine();
		if(crt_machine) crt_machine->setup_output(aspect_ratio);
	}
}

void MultiCRTMachine::close_output() {
	for(const auto &machine: machines_) {
		CRTMachine::Machine *crt_machine = machine->crt_machine();
		if(crt_machine) crt_machine->close_output();
	}
}

Outputs::CRT::CRT *MultiCRTMachine::get_crt() {
	CRTMachine::Machine *crt_machine = machines_.front()->crt_machine();
	return crt_machine ? crt_machine->get_crt() : nullptr;
}

Outputs::Speaker::Speaker *MultiCRTMachine::get_speaker() {
	CRTMachine::Machine *crt_machine = machines_.front()->crt_machine();
	return crt_machine ? crt_machine->get_speaker() : nullptr;
}

void MultiCRTMachine::run_for(const Cycles cycles) {
	for(const auto &machine: machines_) {
		CRTMachine::Machine *crt_machine = machine->crt_machine();
		if(crt_machine) crt_machine->run_for(cycles);
	}

	// TODO: announce an opportunity potentially to reorder the list of machines.
}

double MultiCRTMachine::get_clock_rate() {
	// TODO: something smarter than this? Not all clock rates will necessarily be the same.
	CRTMachine::Machine *crt_machine = machines_.front()->crt_machine();
	return crt_machine ? crt_machine->get_clock_rate() : 0.0;
}

bool MultiCRTMachine::get_clock_is_unlimited() {
	CRTMachine::Machine *crt_machine = machines_.front()->crt_machine();
	return crt_machine ? crt_machine->get_clock_is_unlimited() : false;
}

void MultiCRTMachine::set_delegate(::CRTMachine::Machine::Delegate *delegate) {
	// TODO
}

void MultiCRTMachine::machine_did_change_clock_rate(Machine *machine) {
	// TODO
}

void MultiCRTMachine::machine_did_change_clock_is_unlimited(Machine *machine) {
	// TODO
}
