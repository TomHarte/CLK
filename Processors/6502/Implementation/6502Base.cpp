//
//  6502Base.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../6502.hpp"

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
		case Register::S:						return s_;
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
		case Register::S:				s_ = static_cast<uint8_t>(value);		break;
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
	state.execution_state.cycles_into_phase = cycles_in_phase_;
	state.execution_state.address = address_.full;
	state.execution_state.next_address = next_address_.full;
	if(is_jammed_) {
		state.execution_state.phase = State::ExecutionState::Phase::Jammed;
	} else if(wait_is_active_) {
		state.execution_state.phase = State::ExecutionState::Phase::Waiting;
	} else if(stop_is_active_) {
		state.execution_state.phase = State::ExecutionState::Phase::Stopped;
	} else {
		// Test for the micro-op pointer being inside the reset, IRQ or NMI programs.
		// If not then the only thing left is instruction.
		auto is_in_program = [this](const MicroOp *const op) -> bool {
			if(scheduled_program_counter_ < op) return false;

			const MicroOp *final_op = op;
			while(*final_op != OperationMoveToNextProgram) {
				++final_op;
			}
			return scheduled_program_counter_ < final_op;
		};

		if(is_in_program(get_reset_program())) {
			state.execution_state.phase = State::ExecutionState::Phase::Reset;
		} else if(is_in_program(get_irq_program())) {
			state.execution_state.phase = State::ExecutionState::Phase::IRQ;
		} else if(is_in_program(get_nmi_program())) {
			state.execution_state.phase = State::ExecutionState::Phase::NMI;
		} else {
			state.execution_state.phase = State::ExecutionState::Phase::Instruction;
		}
	}

	return state;
}
