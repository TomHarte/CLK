//
//  AmstradCPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "AmstradCPC.hpp"

using namespace AmstradCPC;

HalfCycles Machine::perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
	return HalfCycles(0);
}

void Machine::flush() {
}

void Machine::setup_output(float aspect_ratio) {
}

void Machine::close_output() {
}

std::shared_ptr<Outputs::CRT::CRT> Machine::get_crt() {
	return nullptr;
}

std::shared_ptr<Outputs::Speaker> Machine::get_speaker() {
	return nullptr;
}

void Machine::run_for(const Cycles cycles) {
}

void Machine::configure_as_target(const StaticAnalyser::Target &target) {
}
