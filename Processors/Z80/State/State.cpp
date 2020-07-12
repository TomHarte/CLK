//
//  State.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "State.hpp"

#include <cassert>

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
	execution_state.refresh_address = src.refresh_addr_.full;
	execution_state.half_cycles_into_step = src.number_of_cycles_.as<int>();

	// Search for the current holder of the scheduled_program_counter_.
#define ContainedBy(x)	(src.scheduled_program_counter_ >= &src.x[0]) && (src.scheduled_program_counter_ < &src.x[src.x.size()])
#define Populate(x, y)	\
	execution_state.phase = ExecutionState::Phase::x;	\
	execution_state.steps_into_phase = int(src.scheduled_program_counter_ - &src.y[0]);

	if(ContainedBy(conditional_call_untaken_program_)) {
		Populate(UntakenConditionalCall, conditional_call_untaken_program_);
	} else if(ContainedBy(reset_program_)) {
		Populate(Reset, reset_program_);
	} else if(ContainedBy(irq_program_[0])) {
		Populate(IRQMode0, irq_program_[0]);
	} else if(ContainedBy(irq_program_[1])) {
		Populate(IRQMode1, irq_program_[1]);
	} else if(ContainedBy(irq_program_[2])) {
		Populate(IRQMode2, irq_program_[2]);
	} else if(ContainedBy(nmi_program_)) {
		Populate(NMI, nmi_program_);
	} else {
		if(src.current_instruction_page_ == &src.base_page_) {
			execution_state.instruction_page = 0;
		} else if(src.current_instruction_page_ == &src.ed_page_) {
			execution_state.instruction_page = 0xed;
		} else if(src.current_instruction_page_ == &src.fd_page_) {
			execution_state.instruction_page = 0xfd;
		} else if(src.current_instruction_page_ == &src.dd_page_) {
			execution_state.instruction_page = 0xdd;
		} else if(src.current_instruction_page_ == &src.cb_page_) {
			execution_state.instruction_page = 0xcb;
		} else if(src.current_instruction_page_ == &src.fdcb_page_) {
			execution_state.instruction_page = 0xfdcb;
		} else if(src.current_instruction_page_ == &src.ddcb_page_) {
			execution_state.instruction_page = 0xddcb;
		}

		if(ContainedBy(current_instruction_page_->fetch_decode_execute)) {
			Populate(FetchDecode, current_instruction_page_->fetch_decode_execute);
		} else {
			// There's no need to determine which opcode because that knowledge is already
			// contained in the dedicated opcode field.
			Populate(Operation, current_instruction_page_->instructions[src.operation_ & src.halt_mask_]);
		}
	}

	assert(execution_state.steps_into_phase >= 0);

#undef Populate
#undef ContainedBy
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
	target.refresh_addr_.full = execution_state.refresh_address;
	target.number_of_cycles_ = HalfCycles(execution_state.half_cycles_into_step);

	switch(execution_state.instruction_page) {
		default:		target.current_instruction_page_ = &target.base_page_;	break;
		case 0xed:		target.current_instruction_page_ = &target.ed_page_;	break;
		case 0xdd:		target.current_instruction_page_ = &target.dd_page_;	break;
		case 0xcb:		target.current_instruction_page_ = &target.cb_page_;	break;
		case 0xfd:		target.current_instruction_page_ = &target.fd_page_;	break;
		case 0xfdcb:	target.current_instruction_page_ = &target.fdcb_page_;	break;
		case 0xddcb:	target.current_instruction_page_ = &target.ddcb_page_;	break;
	}

	switch(execution_state.phase) {
		case ExecutionState::Phase::UntakenConditionalCall:		target.scheduled_program_counter_ = &target.conditional_call_untaken_program_[0];						break;
		case ExecutionState::Phase::Reset:						target.scheduled_program_counter_ = &target.reset_program_[0];											break;
		case ExecutionState::Phase::IRQMode0:					target.scheduled_program_counter_ = &target.irq_program_[0][0];											break;
		case ExecutionState::Phase::IRQMode1:					target.scheduled_program_counter_ = &target.irq_program_[1][0];											break;
		case ExecutionState::Phase::IRQMode2:					target.scheduled_program_counter_ = &target.irq_program_[2][0];											break;
		case ExecutionState::Phase::NMI:						target.scheduled_program_counter_ = &target.nmi_program_[0];											break;
		case ExecutionState::Phase::FetchDecode:				target.scheduled_program_counter_ = &target.current_instruction_page_->fetch_decode_execute[0];			break;
		case ExecutionState::Phase::Operation:					target.scheduled_program_counter_ = target.current_instruction_page_->instructions[target.operation_];	break;
	}
	target.scheduled_program_counter_ += execution_state.steps_into_phase;
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
		DeclareField(refresh_address);

		AnnounceEnum(Phase);
		DeclareField(phase);
		DeclareField(half_cycles_into_step);
		DeclareField(steps_into_phase);
		DeclareField(instruction_page);
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
