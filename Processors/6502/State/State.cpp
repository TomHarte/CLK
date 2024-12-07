//
//  State.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "State.hpp"

using namespace CPU::MOS6502;

State::State(const ProcessorBase &src): State() {
	// Registers.
	registers.program_counter = src.pc_.full;
	registers.stack_pointer = src.s_;
	registers.flags = src.get_flags();
	registers.a = src.a_;
	registers.x = src.x_;
	registers.y = src.y_;

	// Inputs.
	inputs.ready = src.ready_line_is_enabled_;
	inputs.irq = src.irq_line_;
	inputs.nmi = src.nmi_line_is_enabled_;
	inputs.reset = src.interrupt_requests_ & (ProcessorStorage::InterruptRequestFlags::Reset | ProcessorStorage::InterruptRequestFlags::PowerOn);

	// Execution state.
	execution_state.operation = src.operation_;
	execution_state.operand = src.operand_;
	execution_state.address = src.address_.full;
	execution_state.next_address = src.next_address_.full;
	if(src.ready_is_active_) {
		execution_state.phase = State::ExecutionState::Phase::Ready;
	} else if(src.is_jammed_) {
		execution_state.phase = State::ExecutionState::Phase::Jammed;
	} else if(src.wait_is_active_) {
		execution_state.phase = State::ExecutionState::Phase::Waiting;
	} else if(src.stop_is_active_) {
		execution_state.phase = State::ExecutionState::Phase::Stopped;
	} else {
		execution_state.phase = State::ExecutionState::Phase::Instruction;
	}

	const auto micro_offset = size_t(src.scheduled_program_counter_ - &src.operations_[0][0]);
	const auto list_length = sizeof(ProcessorStorage::InstructionList) / sizeof(ProcessorStorage::MicroOp);

	execution_state.micro_program = int(micro_offset / list_length);
	execution_state.micro_program_offset = int(micro_offset % list_length);
	assert(&src.operations_[execution_state.micro_program][execution_state.micro_program_offset] == src.scheduled_program_counter_);
}

void State::apply(ProcessorBase &target) {
	// Registers.
	target.pc_.full = registers.program_counter;
	target.s_ = registers.stack_pointer;
	target.set_flags(registers.flags);
	target.a_ = registers.a;
	target.x_ = registers.x;
	target.y_ = registers.y;

	// Inputs.
	target.ready_line_is_enabled_ = inputs.ready;
	target.set_irq_line(inputs.irq);
	target.set_nmi_line(inputs.nmi);
	target.set_reset_line(inputs.reset);

	// Execution state.
	target.ready_is_active_ = target.is_jammed_ = target.wait_is_active_ = target.stop_is_active_ = false;
	switch(execution_state.phase) {
		case State::ExecutionState::Phase::Ready:	target.ready_is_active_ = true;		break;
		case State::ExecutionState::Phase::Jammed:	target.is_jammed_ = true;			break;
		case State::ExecutionState::Phase::Stopped:	target.stop_is_active_ = true;		break;
		case State::ExecutionState::Phase::Waiting:	target.wait_is_active_ = true;		break;
		case State::ExecutionState::Phase::Instruction:									break;
	}

	target.operation_ = execution_state.operation;
	target.operand_ = execution_state.operand;
	target.address_.full = execution_state.address;
	target.next_address_.full = execution_state.next_address;
	target.scheduled_program_counter_ = &target.operations_[execution_state.micro_program][execution_state.micro_program_offset];
}

// Boilerplate follows here, to establish 'reflection'.
void State::declare_fields() {
	DeclareField(registers);
	DeclareField(execution_state);
	DeclareField(inputs);
}

void State::Registers::declare_fields() {
	DeclareField(program_counter);
	DeclareField(stack_pointer);
	DeclareField(flags);
	DeclareField(a);
	DeclareField(x);
	DeclareField(y);
}

void State::ExecutionState::declare_fields() {
	AnnounceEnum(Phase);
	DeclareField(phase);
	DeclareField(micro_program);
	DeclareField(micro_program_offset);
	DeclareField(operation);
	DeclareField(operand);
	DeclareField(address);
	DeclareField(next_address);
}

void State::Inputs::declare_fields() {
	DeclareField(ready);
	DeclareField(irq);
	DeclareField(nmi);
	DeclareField(reset);
}
