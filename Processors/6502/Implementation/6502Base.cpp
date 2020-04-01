//
//  6502Base.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../6502.hpp"

#include <cassert>

using namespace CPU::MOS6502;

const uint8_t CPU::MOS6502::JamOpcode = 0xf2;

uint16_t ProcessorBase::get_value_of_register(Register r) {
	switch (r) {
		case Register::ProgramCounter:			return pc_.full;
		case Register::LastOperationAddress:	return last_operation_pc_.full;
		case Register::StackPointer:			return s_;
		case Register::Flags:					return get_flags();
		case Register::A:						return a_;
		case Register::X:						return x_;
		case Register::Y:						return y_;
		default: return 0;
	}
}

void ProcessorBase::set_value_of_register(Register r, uint16_t value) {
	switch (r) {
		case Register::ProgramCounter:	pc_.full = value;						break;
		case Register::StackPointer:	s_ = static_cast<uint8_t>(value);		break;
		case Register::Flags:			set_flags(static_cast<uint8_t>(value));	break;
		case Register::A:				a_ = static_cast<uint8_t>(value);		break;
		case Register::X:				x_ = static_cast<uint8_t>(value);		break;
		case Register::Y:				y_ = static_cast<uint8_t>(value);		break;
		default: break;
	}
}

bool ProcessorBase::is_jammed() {
	return is_jammed_;
}

ProcessorBase::State ProcessorBase::get_state() {
	ProcessorBase::State state;

	// Fill in registers.
	state.registers.program_counter = pc_.full;
	state.registers.stack_pointer = s_;
	state.registers.flags = get_flags();
	state.registers.a = a_;
	state.registers.x = x_;
	state.registers.y = y_;

	// Fill in other inputs.
	state.inputs.ready = ready_line_is_enabled_;
	state.inputs.irq = irq_line_;
	state.inputs.nmi = nmi_line_is_enabled_;
	state.inputs.reset = interrupt_requests_ & (InterruptRequestFlags::Reset | InterruptRequestFlags::PowerOn);

	// Fill in execution state.
	state.execution_state.operation = operation_;
	state.execution_state.operand = operand_;
	state.execution_state.address = address_.full;
	state.execution_state.next_address = next_address_.full;
	if(ready_is_active_) {
		state.execution_state.phase = State::ExecutionState::Phase::Ready;
	} else if(is_jammed_) {
		state.execution_state.phase = State::ExecutionState::Phase::Jammed;
	} else if(wait_is_active_) {
		state.execution_state.phase = State::ExecutionState::Phase::Waiting;
	} else if(stop_is_active_) {
		state.execution_state.phase = State::ExecutionState::Phase::Stopped;
	} else {
		state.execution_state.phase = State::ExecutionState::Phase::Instruction;
	}

	const auto micro_offset = size_t(scheduled_program_counter_ - &operations_[0][0]);
	const auto list_length = sizeof(InstructionList) / sizeof(MicroOp);

	state.execution_state.micro_program = int(micro_offset / list_length);
	state.execution_state.micro_program_offset = int(micro_offset % list_length);
	assert(&operations_[state.execution_state.micro_program][state.execution_state.micro_program_offset] == scheduled_program_counter_);

	return state;
}

void ProcessorBase::set_state(const State &state) {
	// Grab registers.
	pc_.full = state.registers.program_counter;
	s_ = state.registers.stack_pointer;
	set_flags(state.registers.flags);
	a_ = state.registers.a;
	x_ = state.registers.x;
	y_ = state.registers.y;

	// Grab other inputs.
	ready_line_is_enabled_ = state.inputs.ready;
	set_irq_line(state.inputs.irq);
	set_nmi_line(state.inputs.nmi);
	set_reset_line(state.inputs.reset);

	// Set execution state.
	ready_is_active_ = is_jammed_ = wait_is_active_ = stop_is_active_ = false;
	switch(state.execution_state.phase) {
		case State::ExecutionState::Phase::Ready:	ready_is_active_ = true;	break;
		case State::ExecutionState::Phase::Jammed:	is_jammed_ = true;			break;
		case State::ExecutionState::Phase::Stopped:	stop_is_active_ = true;		break;
		case State::ExecutionState::Phase::Waiting:	wait_is_active_ = true;		break;
		case State::ExecutionState::Phase::Instruction:							break;
	}

	operation_ = state.execution_state.operation;
	operand_ = state.execution_state.operand;
	address_.full = state.execution_state.address;
	next_address_.full = state.execution_state.next_address;
	scheduled_program_counter_ = &operations_[state.execution_state.micro_program][state.execution_state.micro_program_offset];
}
