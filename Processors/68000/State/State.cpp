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
	// Registers.
	for(int c = 0; c < 7; ++c) {
		registers.address[c] = src.address_[c].full;
		registers.data[c] = src.data_[c].full;
	}
	registers.data[7] = src.data_[7].full;
	registers.user_stack_pointer = src.is_supervisor_ ? src.stack_pointers_[0].full : src.address_[7].full;
	registers.supervisor_stack_pointer = src.is_supervisor_ ? src.address_[7].full : src.stack_pointers_[1].full;
	registers.status = src.get_status();
	registers.program_counter = src.program_counter_.full;
	registers.prefetch = src.prefetch_queue_.full;
	registers.instruction = src.decoded_instruction_.full;

	// Inputs.
	inputs.bus_interrupt_level = uint8_t(src.bus_interrupt_level_);
	inputs.dtack = src.dtack_;
	inputs.is_peripheral_address = src.is_peripheral_address_;
	inputs.bus_error = src.bus_error_;
	inputs.bus_request = src.bus_request_;
	inputs.bus_grant = false;	// TODO (within the 68000).
	inputs.halt = src.halt_;

	// Execution state.
	execution_state.e_clock_phase = src.e_clock_phase_.as<uint8_t>();
	execution_state.effective_address[0] = src.effective_address_[0].full;
	execution_state.effective_address[1] = src.effective_address_[1].full;
	execution_state.source_data = src.source_bus_data_.full;
	execution_state.destination_data = src.destination_bus_data_.full;
	execution_state.last_trace_flag = src.last_trace_flag_;
	execution_state.next_word = src.next_word_;
	execution_state.dbcc_false_address = src.dbcc_false_address_;
	execution_state.is_starting_interrupt = src.is_starting_interrupt_;
	execution_state.pending_interrupt_level = uint8_t(src.pending_interrupt_level_);
	execution_state.accepted_interrupt_level = uint8_t(src.accepted_interrupt_level_);
	execution_state.movem_final_address = src.movem_final_address_;

	static_assert(sizeof(execution_state.source_addresses) == sizeof(src.precomputed_addresses_));
	memcpy(&execution_state.source_addresses, &src.precomputed_addresses_, sizeof(src.precomputed_addresses_));

	// This is collapsed to a Boolean; if there is an active program then it's the
	// one implied by the current instruction.
	execution_state.active_program = src.active_program_;

	// Slightly dodgy assumption here: the Phase enum will always exactly track
	// the 68000's ExecutionState enum.
	execution_state.phase = ExecutionState::Phase(src.execution_state_);

	auto contained_by = [](const auto *source, const auto *reference) -> bool {
		while(true) {
			if(source == reference) return true;
			if(source->is_terminal()) return false;
			++source;
		}
	};

	// Store enough information to relocate the MicroOp.
	const ProcessorBase::MicroOp *micro_op_base = nullptr;
	if(src.active_program_) {
		micro_op_base = &src.all_micro_ops_[src.instructions[src.decoded_instruction_.full].micro_operations];
		assert(contained_by(micro_op_base, src.active_micro_op_));
		execution_state.micro_op_source = ExecutionState::MicroOpSource::ActiveProgram;
	} else {
		if(contained_by(src.long_exception_micro_ops_, src.active_micro_op_)) {
			execution_state.micro_op_source = ExecutionState::MicroOpSource::LongException;
			micro_op_base = src.long_exception_micro_ops_;
		} else if(contained_by(src.short_exception_micro_ops_, src.active_micro_op_)) {
			execution_state.micro_op_source = ExecutionState::MicroOpSource::ShortException;
			micro_op_base = src.short_exception_micro_ops_;
		} else if(contained_by(src.interrupt_micro_ops_, src.active_micro_op_)) {
			execution_state.micro_op_source = ExecutionState::MicroOpSource::Interrupt;
			micro_op_base = src.interrupt_micro_ops_;
		} else {
			assert(false);
		}
	}
	execution_state.micro_op = uint8_t(src.active_micro_op_ - micro_op_base);

	// Encode the BusStep.
	struct BusStepOption {
		const ProcessorBase::BusStep *const base;
		const ExecutionState::BusStepSource source;
	};
	BusStepOption bus_step_options[] =  {
		{
			src.reset_bus_steps_,
			ExecutionState::BusStepSource::Reset
		},
		{
			src.branch_taken_bus_steps_,
			ExecutionState::BusStepSource::BranchTaken
		},
		{
			src.branch_byte_not_taken_bus_steps_,
			ExecutionState::BusStepSource::BranchByteNotTaken
		},
		{
			src.branch_word_not_taken_bus_steps_,
			ExecutionState::BusStepSource::BranchWordNotTaken
		},
		{
			src.bsr_bus_steps_,
			ExecutionState::BusStepSource::BSR
		},
		{
			src.dbcc_condition_true_steps_,
			ExecutionState::BusStepSource::DBccConditionTrue
		},
		{
			src.dbcc_condition_false_no_branch_steps_,
			ExecutionState::BusStepSource::DBccConditionFalseNoBranch
		},
		{
			src.dbcc_condition_false_branch_steps_,
			ExecutionState::BusStepSource::DBccConditionFalseBranch
		},
		{
			src.movem_read_steps_,
			ExecutionState::BusStepSource::MovemRead
		},
		{
			src.movem_write_steps_,
			ExecutionState::BusStepSource::MovemWrite
		},
		{
			src.trap_steps_,
			ExecutionState::BusStepSource::Trap
		},
		{
			src.bus_error_steps_,
			ExecutionState::BusStepSource::BusError
		},
		{
			&src.all_bus_steps_[src.active_micro_op_->bus_program],
			ExecutionState::BusStepSource::FollowMicroOp
		},
		{nullptr}
	};
	const BusStepOption *bus_step_option = bus_step_options;
	const ProcessorBase::BusStep *bus_step_base = nullptr;
	while(bus_step_option->base) {
		if(contained_by(bus_step_option->base, src.active_step_)) {
			bus_step_base = bus_step_option->base;
			execution_state.bus_step_source = bus_step_option->source;
			break;
		}
		++bus_step_option;
	}
	assert(bus_step_base);
	execution_state.bus_step = uint8_t(src.active_step_ - bus_step_base);
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
		DeclareField(user_stack_pointer);
		DeclareField(supervisor_stack_pointer);
		DeclareField(status);
		DeclareField(program_counter);
		DeclareField(prefetch);
		DeclareField(instruction);
	}
}

State::Inputs::Inputs() {
	if(needs_declare()) {
		DeclareField(bus_interrupt_level);
		DeclareField(dtack);
		DeclareField(is_peripheral_address);
		DeclareField(bus_error);
		DeclareField(bus_request);
		DeclareField(bus_grant);
		DeclareField(halt);
	}
}

State::ExecutionState::ExecutionState() {
	if(needs_declare()) {
		DeclareField(e_clock_phase);
		DeclareField(effective_address);
		DeclareField(source_data);
		DeclareField(destination_data);
		DeclareField(last_trace_flag);
		DeclareField(next_word);
		DeclareField(dbcc_false_address);
		DeclareField(is_starting_interrupt);
		DeclareField(pending_interrupt_level);
		DeclareField(accepted_interrupt_level);
		DeclareField(active_program);
		DeclareField(movem_final_address);
		DeclareField(source_addresses);

		AnnounceEnum(Phase);
		DeclareField(phase);

		AnnounceEnum(MicroOpSource);
		DeclareField(micro_op_source);
		DeclareField(micro_op);

		AnnounceEnum(BusStepSource);
		DeclareField(bus_step_source);
		DeclareField(bus_step);
	}
}
