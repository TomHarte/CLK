//
//  State.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "State.hpp"

using namespace CPU::Z80;

State::State(const ProcessorBase &src): State() {
	// Registers.
	registers.a = src.a_;
	registers.flags = src.get_flags();
	registers.bc = src.bc_.full;
	registers.de = src.de_.full;
	registers.hl = src.hl_.full;
	registers.bcDash = src.bcDash_.full;
	registers.deDash = src.deDash_.full;
	registers.hlDash = src.hlDash_.full;
	registers.ix = src.ix_.full;
	registers.iy = src.iy_.full;
	registers.ir = src.ir_.full;
	registers.program_counter = src.pc_.full;
	registers.stack_pointer = src.sp_.full;
	registers.memptr = src.memptr_.full;
	registers.interrupt_mode = src.interrupt_mode_;
	registers.iff1 = src.iff1_;
	registers.iff2 = src.iff2_;

	// Inputs.
	inputs.irq = src.irq_line_;
	inputs.nmi = src.nmi_line_;
	inputs.wait = src.wait_line_;
	inputs.bus_request = src.bus_request_line_;
}

void State::apply(ProcessorBase &target) {
	// Registers.
	target.a_ = registers.a;
	target.set_flags(registers.flags);
	target.bc_.full = registers.bc;
	target.de_.full = registers.de;
	target.hl_.full = registers.hl;
	target.bcDash_.full = registers.bcDash;
	target.deDash_.full = registers.deDash;
	target.hlDash_.full = registers.hlDash;
	target.ix_.full = registers.ix;
	target.iy_.full = registers.iy;
	target.ir_.full = registers.ir;
	target.pc_.full = registers.program_counter;
	target.sp_.full = registers.stack_pointer;
	target.memptr_.full = registers.memptr;
	target.interrupt_mode_ = registers.interrupt_mode;
	target.iff1_ = registers.iff1;
	target.iff2_ = registers.iff2;

	// Inputs.
	target.irq_line_ = inputs.irq;
	target.nmi_line_ = inputs.nmi;
	target.wait_line_ = inputs.wait;
	target.bus_request_line_ = inputs.bus_request;
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
		DeclareField(a);
		DeclareField(flags);
		DeclareField(bc);
		DeclareField(de);
		DeclareField(hl);
		DeclareField(bcDash);
		DeclareField(deDash);
		DeclareField(hlDash);
		DeclareField(ix);
		DeclareField(iy);
		DeclareField(ir);
		DeclareField(program_counter);
		DeclareField(stack_pointer);
		DeclareField(interrupt_mode);
		DeclareField(iff1);
		DeclareField(iff2);
		DeclareField(memptr);
	}
}

State::ExecutionState::ExecutionState() {
	if(needs_declare()) {
	}
}

State::Inputs::Inputs() {
	if(needs_declare()) {
		DeclareField(irq);
		DeclareField(nmi);
		DeclareField(bus_request);
		DeclareField(wait);
	}
}
