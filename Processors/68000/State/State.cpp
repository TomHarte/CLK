//
//  State.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "State.hpp"

using namespace CPU::MC68000;

State::State(const ProcessorBase &src): State() {
}

void State::apply(ProcessorBase &target) {
}

// Boilerplate follows here, to establish 'reflection'.
State::State() {
	if(needs_declare()) {
		DeclareField(registers);
		DeclareField(execution_state);
		DeclareField(inputs);
	}
}

State::Registers::Registers() {
	if(needs_declare()) {
		DeclareField(data);
		DeclareField(address);
	}
}

State::ExecutionState::ExecutionState() {
	if(needs_declare()) {
	}
}

State::Inputs::Inputs() {
	if(needs_declare()) {
	}
}
