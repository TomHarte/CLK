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

	// Execution State.
	execution_state.is_halted = src.halt_mask_ == 0x00;
	execution_state.requests = src.request_status_;
	execution_state.last_requests = src.last_request_status_;
	execution_state.temp8 = src.temp8_;
	execution_state.temp16 = src.temp16_.full;
	execution_state.operation = src.operation_;
	execution_state.flag_adjustment_history = src.flag_adjustment_history_;
	execution_state.pc_increment = src.pc_increment_;
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

	// Execution State.
	target.halt_mask_ = execution_state.is_halted ? 0x00 : 0xff;
	target.request_status_ = execution_state.requests;
	target.last_request_status_ = execution_state.last_requests;
	target.temp8_ = execution_state.temp8;
	target.temp16_.full = execution_state.temp16;
	target.operation_ = execution_state.operation;
	target.flag_adjustment_history_ = execution_state.flag_adjustment_history;
	target.pc_increment_ = execution_state.pc_increment;

	// TODO: scheduled_program_counter_ and number_of_cycles_.
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
		DeclareField(is_halted);
		DeclareField(requests);
		DeclareField(last_requests);
		DeclareField(temp8);
		DeclareField(operation);
		DeclareField(temp16);
		DeclareField(flag_adjustment_history);
		DeclareField(pc_increment);
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
