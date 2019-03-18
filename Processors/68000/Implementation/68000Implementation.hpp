//
//  68000Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

template <class T, bool dtack_is_implicit> void Processor<T, dtack_is_implicit>::run_for(HalfCycles duration) {
	// TODO: obey the 'cycles' count.
	while(true) {

		/*
			PERFORM THE CURRENT BUS STEP'S MICROCYCLE.
		*/
			// Check for DTack if this isn't being treated implicitly.
			if(!dtack_is_implicit) {
				if(active_step_->microcycle.data_select_active() && !dtack_) {
					// TODO: perform wait state.
					continue;
				}
			}

			// TODO: synchronous bus.

			// TODO: check for bus error.

			// Perform the microcycle.
			bus_handler_.perform_bus_operation(active_step_->microcycle, is_supervisor_);

		/*
			PERFORM THE BUS STEP'S ACTION.
		*/
			if(!active_step_->is_terminal()) {
				switch(active_step_->action) {
					default:
						std::cerr << "Unimplemented 68000 bus step action: " << int(active_step_->action) << std::endl;
						return;
					break;

					case BusStep::Action::None: break;

					case BusStep::Action::IncrementEffectiveAddress:	effective_address_ += 2;	break;
					case BusStep::Action::IncrementProgramCounter:		program_counter_.full += 2;	break;

					case BusStep::Action::AdvancePrefetch:
						prefetch_queue_[0] = prefetch_queue_[1];
					break;
				}

				// Move to the next bus step.
				++ active_step_;

				// Skip the micro-op renavigation below.
				continue;
			}

		/*
			FIND THE NEXT MICRO-OP.
		*/
			while(true) {
				// If there are any more micro-operations available, just move onwards.
				if(active_micro_op_ && !active_micro_op_->is_terminal()) {
					++active_micro_op_;
				} else {
					// Either the micro-operations for this instruction have been exhausted, or
					// no instruction was ongoing. Either way, do a standard instruction operation.

					// TODO: unless an interrupt is pending, or the trap flag is set.

					const uint16_t next_instruction = prefetch_queue_[0].full;
					if(!instructions[next_instruction].micro_operations) {
						// TODO: once all instructions are implemnted, this should be an instruction error.
						std::cerr << "68000 Abilities exhausted; can't manage instruction " << std::hex << next_instruction << std::endl;
						return;
					}

					active_program_ = &instructions[next_instruction];
					active_micro_op_ = active_program_->micro_operations;
				}

				switch(active_micro_op_->action) {
					case MicroOp::Action::None: break;

					case MicroOp::Action::PerformOperation:
						switch(active_program_->operation) {
							case Operation::ABCD: {
								// Pull out the two halves, for simplicity.
								const uint8_t source = active_program_->source->halves.low.halves.low;
								const uint8_t destination = active_program_->destination->halves.low.halves.low;

								// Perform the BCD add by evaluating the two nibbles separately.
								int result = (destination & 0xf) + (source & 0xf) + (extend_flag_ ? 1 : 0);
								if(result > 0x9) result += 0x06;
								result += (destination & 0xf0) + (source & 0xf0);
								if((result&0xff0) > 0x90) result += 0x60;

								// Set all flags essentially as if this were normal addition.
								zero_flag_ |= result & 0xff;
								extend_flag_ = carry_flag_ = result & ~0xff;
								negative_flag_ = result & 0x80;
								overflow_flag_ = ~(source ^ destination) & (destination ^ result) & 0x80;

								// Store the result.
								active_program_->destination->halves.low.halves.low = uint8_t(result);
							} break;

							case Operation::SBCD: {
								// Pull out the two halves, for simplicity.
								const uint8_t source = active_program_->source->halves.low.halves.low;
								const uint8_t destination = active_program_->destination->halves.low.halves.low;

								// Perform the BCD add by evaluating the two nibbles separately.
								int result = (destination & 0xf) - (source & 0xf) - (extend_flag_ ? 1 : 0);
								if(result > 0x9) result -= 0x06;
								result += (destination & 0xf0) - (source & 0xf0);
								if((result&0xff0) > 0x90) result -= 0x60;

								// Set all flags essentially as if this were normal subtraction.
								zero_flag_ |= result & 0xff;
								extend_flag_ = carry_flag_ = result & ~0xff;
								negative_flag_ = result & 0x80;
								overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80;

								// Store the result.
								active_program_->destination->halves.low.halves.low = uint8_t(result);
							} break;

							case Operation::MOVEb:
								zero_flag_ = active_program_->destination->halves.low.halves.low = active_program_->source->halves.low.halves.low;
								negative_flag_ = zero_flag_ & 0x80;
							break;

							case Operation::MOVEw:
								zero_flag_ = active_program_->destination->halves.low.full = active_program_->source->halves.low.full;
								negative_flag_ = zero_flag_ & 0x8000;
							break;

							case Operation::MOVEl:
								zero_flag_ = active_program_->destination->full = active_program_->source->full;
								negative_flag_ = zero_flag_ & 0x80000000;
							break;

							default:
								std::cerr << "Should do something with program operation " << int(active_program_->operation) << std::endl;
							break;
						}
					break;

					case MicroOp::Action::PredecrementSourceAndDestination1:
						-- active_program_->source->full;
						-- active_program_->destination->full;
					break;

					case MicroOp::Action::PredecrementSourceAndDestination2:
						active_program_->source->full -= 2;
						active_program_->destination->full -= 2;
					break;

					case MicroOp::Action::PredecrementSourceAndDestination4:
						active_program_->source->full -= 4;
						active_program_->destination->full -= 4;
					break;

					case MicroOp::Action::SignExtendDestinationWord:
						active_program_->destination->halves.high.full =
							(active_program_->destination->halves.low.full & 0x8000) ? 0xffff : 0x0000;
					break;
				}

				// If we've got to a micro-op that includes bus steps, break out of this loop.
				if(!active_micro_op_->is_terminal()) {
					active_step_ = active_micro_op_->bus_program;
					break;
				}
			}
	}
}

template <class T, bool dtack_is_implicit> ProcessorState Processor<T, dtack_is_implicit>::get_state() {
	write_back_stack_pointer();

	State state;
	memcpy(state.data, data_, sizeof(state.data));
	memcpy(state.address, address_, sizeof(state.address));
	state.user_stack_pointer = stack_pointers_[0].full;
	state.supervisor_stack_pointer = stack_pointers_[1].full;

	// TODO: status word.

	return state;
}

template <class T, bool dtack_is_implicit> void Processor<T, dtack_is_implicit>::set_state(const ProcessorState &state) {
	memcpy(data_, state.data, sizeof(state.data));
	memcpy(address_, state.address, sizeof(state.address));
	stack_pointers_[0].full = state.user_stack_pointer;
	stack_pointers_[1].full = state.supervisor_stack_pointer;

	// TODO: update address[7], once there's a status word to discern the mode this processor should now be in.
}
