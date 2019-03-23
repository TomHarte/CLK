//
//  68000Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

template <class T, bool dtack_is_implicit> void Processor<T, dtack_is_implicit>::run_for(HalfCycles duration) {
	HalfCycles remaining_duration = duration + half_cycles_left_to_run_;
	while(remaining_duration > HalfCycles(0)) {
		/*
			FIND THE NEXT MICRO-OP IF UNKNOWN.
		*/
			if(active_step_->is_terminal()) {
				while(true) {
					// If there are any more micro-operations available, just move onwards.
					if(active_micro_op_ && !active_micro_op_->is_terminal()) {
						++active_micro_op_;
					} else {
						// Either the micro-operations for this instruction have been exhausted, or
						// no instruction was ongoing. Either way, do a standard instruction operation.

						// TODO: unless an interrupt is pending, or the trap flag is set.

						const uint16_t next_instruction = prefetch_queue_.halves.high.full;
						if(!instructions[next_instruction].micro_operations) {
							// TODO: once all instructions are implemnted, this should be an instruction error.
							std::cerr << "68000 Abilities exhausted; can't manage instruction " << std::hex << next_instruction << std::endl;
							return;
						}

						active_program_ = &instructions[next_instruction];
						active_micro_op_ = active_program_->micro_operations;
					}

					switch(active_micro_op_->action) {
						default:
							std::cerr << "Unhandled 68000 micro op action " << std::hex << active_micro_op_->action << std::endl;
						break;

						case int(MicroOp::Action::None): break;

						case int(MicroOp::Action::PerformOperation):
							switch(active_program_->operation) {
								case Operation::ABCD: {
									// Pull out the two halves, for simplicity.
									const uint8_t source = active_program_->source->halves.low.halves.low;
									const uint8_t destination = active_program_->destination->halves.low.halves.low;

									// Perform the BCD add by evaluating the two nibbles separately.
									int result = (destination & 0xf) + (source & 0xf) + (extend_flag_ ? 1 : 0);
									if(result > 0x09) result += 0x06;
									result += (destination & 0xf0) + (source & 0xf0);
									if(result > 0x99) result += 0x60;

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
									if(result > 0x09) result -= 0x06;
									result += (destination & 0xf0) - (source & 0xf0);
									if(result > 0x99) result -= 0x60;

									// Set all flags essentially as if this were normal subtraction.
									zero_flag_ |= result & 0xff;
									extend_flag_ = carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80;

									// Store the result.
									active_program_->destination->halves.low.halves.low = uint8_t(result);
								} break;

								/*
									MOVE.b, MOVE.l and MOVE.w: move the least significant byte or word, or the entire long word,
									and set negative, zero, overflow and carry as appropriate.
								*/
								case Operation::MOVEb:
									zero_flag_ = active_program_->destination->halves.low.halves.low = active_program_->source->halves.low.halves.low;
									negative_flag_ = zero_flag_ & 0x80;
									overflow_flag_ = carry_flag_ = 0;
								break;

								case Operation::MOVEw:
									zero_flag_ = active_program_->destination->halves.low.full = active_program_->source->halves.low.full;
									negative_flag_ = zero_flag_ & 0x8000;
									overflow_flag_ = carry_flag_ = 0;
								break;

								case Operation::MOVEl:
									zero_flag_ = active_program_->destination->full = active_program_->source->full;
									negative_flag_ = zero_flag_ & 0x80000000;
									overflow_flag_ = carry_flag_ = 0;
								break;

								/*
									MOVEA.l: move the entire long word;
									MOVEA.w: move the least significant word and sign extend it.
									Neither sets any flags.
								*/

								case Operation::MOVEAw:
									active_program_->destination->halves.low.full = active_program_->source->halves.low.full;
									active_program_->destination->halves.high.full = (active_program_->destination->halves.low.full & 0x8000) ? 0xffff : 0;
								break;

								case Operation::MOVEAl:
									active_program_->destination->full = active_program_->source->full;
								break;

								default:
									std::cerr << "Should do something with program operation " << int(active_program_->operation) << std::endl;
								break;
							}
						break;

						case int(MicroOp::Action::SetMoveFlagsb):
							zero_flag_ = active_program_->source->halves.low.halves.low;
							negative_flag_ = zero_flag_ & 0x80;
							overflow_flag_ = carry_flag_ = 0;
						break;

						case int(MicroOp::Action::SetMoveFlagsw):
							zero_flag_ = active_program_->source->halves.low.full;
							negative_flag_ = zero_flag_ & 0x8000;
							overflow_flag_ = carry_flag_ = 0;
						break;

						case int(MicroOp::Action::SetMoveFlagsl):
							zero_flag_ = active_program_->source->full;
							negative_flag_ = zero_flag_ & 0x80000000;
							overflow_flag_ = carry_flag_ = 0;
						break;

						case int(MicroOp::Action::Decrement1):
							if(active_micro_op_->action & MicroOp::SourceMask)		active_program_->source->full -= 1;
							if(active_micro_op_->action & MicroOp::DestinationMask)	active_program_->destination->full -= 1;
						break;

						case int(MicroOp::Action::Decrement2):
							if(active_micro_op_->action & MicroOp::SourceMask)		active_program_->source->full -= 2;
							if(active_micro_op_->action & MicroOp::DestinationMask)	active_program_->destination->full -= 2;
						break;

						case int(MicroOp::Action::Decrement4):
							if(active_micro_op_->action & MicroOp::SourceMask)		active_program_->source->full -= 4;
							if(active_micro_op_->action & MicroOp::DestinationMask)	active_program_->destination->full -= 4;
						break;

						case int(MicroOp::Action::Increment1):
							if(active_micro_op_->action & MicroOp::SourceMask)		active_program_->source->full += 1;
							if(active_micro_op_->action & MicroOp::DestinationMask)	active_program_->destination->full += 1;
						break;

						case int(MicroOp::Action::Increment2):
							if(active_micro_op_->action & MicroOp::SourceMask)		active_program_->source->full += 2;
							if(active_micro_op_->action & MicroOp::DestinationMask)	active_program_->destination->full += 2;
						break;

						case int(MicroOp::Action::Increment4):
							if(active_micro_op_->action & MicroOp::SourceMask)		active_program_->source->full += 4;
							if(active_micro_op_->action & MicroOp::DestinationMask)	active_program_->destination->full += 4;
						break;

						case int(MicroOp::Action::SignExtendWord):
							if(active_micro_op_->action & MicroOp::SourceMask) {
								active_program_->source->halves.high.full =
									(active_program_->source->halves.low.full & 0x8000) ? 0xffff : 0x0000;
							}
							if(active_micro_op_->action & MicroOp::DestinationMask) {
								active_program_->destination->halves.high.full =
									(active_program_->destination->halves.low.full & 0x8000) ? 0xffff : 0x0000;
							}
						break;

						case int(MicroOp::Action::SignExtendByte):
							if(active_micro_op_->action & MicroOp::SourceMask) {
								active_program_->source->full = (active_program_->source->full & 0xff) |
									(active_program_->source->full & 0x80) ? 0xffffff : 0x000000;
							}
							if(active_micro_op_->action & MicroOp::DestinationMask) {
								active_program_->destination->full = (active_program_->destination->full & 0xff) |
									(active_program_->destination->full & 0x80) ? 0xffffff : 0x000000;
							}
						break;

						case int(MicroOp::Action::CalcD16An) | MicroOp::SourceMask:
							effective_address_[0] = int16_t(prefetch_queue_.halves.low.full) + active_program_->source->full;
						break;

						case int(MicroOp::Action::CalcD16An) | MicroOp::DestinationMask:
							effective_address_[1] = int16_t(prefetch_queue_.halves.low.full) + active_program_->destination->full;
						break;

						case int(MicroOp::Action::CalcD16An) | MicroOp::SourceMask | MicroOp::DestinationMask:
							effective_address_[0] = int16_t(prefetch_queue_.halves.high.full) + active_program_->source->full;
							effective_address_[1] = int16_t(prefetch_queue_.halves.low.full) + active_program_->destination->full;
						break;

						// TODO: permit as below for DestinationMask and SourceMask|DestinationMask; would prefer to test first.
#define CalculateD8AnXn(data, source, target)	{\
	const auto register_index = (data.full >> 12) & 7;	\
	const RegisterPair32 &displacement = (data.full & 0x8000) ? address_[register_index] : data_[register_index];	\
	target = int8_t(data.halves.low) + source->full;	\
\
	if(data.full & 0x800) {	\
		effective_address_[0] += displacement.halves.low.full;	\
	} else {	\
		effective_address_[0] += displacement.full;	\
	}	\
}
						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::SourceMask: {
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->source, effective_address_[0]);
						} break;

						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->destination, effective_address_[1]);
						} break;

						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::SourceMask | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.high, active_program_->source, effective_address_[0]);
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->destination, effective_address_[1]);
						} break;

#undef CalculateD8AnXn

						case int(MicroOp::Action::AssembleWordFromPrefetch) | MicroOp::SourceMask:
							effective_address_[0] = prefetch_queue_.halves.high.full;
						break;

						case int(MicroOp::Action::AssembleWordFromPrefetch) | MicroOp::DestinationMask:
							effective_address_[1] = prefetch_queue_.halves.high.full;
						break;

						case int(MicroOp::Action::AssembleLongWordFromPrefetch) | MicroOp::SourceMask:
							effective_address_[0] = prefetch_queue_.full;
						break;

						case int(MicroOp::Action::AssembleLongWordFromPrefetch) | MicroOp::DestinationMask:
							effective_address_[1] = prefetch_queue_.full;
						break;
					}

					// If we've got to a micro-op that includes bus steps, break out of this loop.
					if(!active_micro_op_->is_terminal()) {
						active_step_ = active_micro_op_->bus_program;
						break;
					}
				}
			}


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
			remaining_duration -=
				active_step_->microcycle.length +
				bus_handler_.perform_bus_operation(active_step_->microcycle, is_supervisor_);


		/*
			PERFORM THE BUS STEP'S ACTION.
		*/
			switch(active_step_->action) {
				default:
					std::cerr << "Unimplemented 68000 bus step action: " << int(active_step_->action) << std::endl;
					return;
				break;

				case BusStep::Action::None: break;

				case BusStep::Action::IncrementEffectiveAddress0:	effective_address_[0] += 2;	break;
				case BusStep::Action::IncrementEffectiveAddress1:	effective_address_[1] += 2;	break;
				case BusStep::Action::IncrementProgramCounter:		program_counter_.full += 2;	break;

				case BusStep::Action::AdvancePrefetch:
					prefetch_queue_.halves.high = prefetch_queue_.halves.low;
				break;
			}

			// Move to the next bus step.
			++ active_step_;
	}

	half_cycles_left_to_run_ = remaining_duration;
}

template <class T, bool dtack_is_implicit> ProcessorState Processor<T, dtack_is_implicit>::get_state() {
	write_back_stack_pointer();

	State state;
	memcpy(state.data, data_, sizeof(state.data));
	memcpy(state.address, address_, sizeof(state.address));
	state.user_stack_pointer = stack_pointers_[0].full;
	state.supervisor_stack_pointer = stack_pointers_[1].full;

	// TODO: rest of status word: interrupt level, trace flag.
	state.status =
		(carry_flag_ 	? 0x0001 : 0x0000) |
		(overflow_flag_	? 0x0002 : 0x0000) |
		(zero_flag_		? 0x0000 : 0x0004) |
		(negative_flag_	? 0x0008 : 0x0000) |
		(extend_flag_	? 0x0010 : 0x0000) |

		(is_supervisor_ << 13);

	return state;
}

template <class T, bool dtack_is_implicit> void Processor<T, dtack_is_implicit>::set_state(const ProcessorState &state) {
	memcpy(data_, state.data, sizeof(state.data));
	memcpy(address_, state.address, sizeof(state.address));
	stack_pointers_[0].full = state.user_stack_pointer;
	stack_pointers_[1].full = state.supervisor_stack_pointer;

	carry_flag_		= state.status & 0x0001;
	overflow_flag_	= state.status & 0x0002;
	zero_flag_		= (state.status & 0x0004) ^ 0x0004;
	negative_flag_	= state.status & 0x0008;
	extend_flag_	= state.status & 0x0010;

	is_supervisor_	= (state.status >> 13) & 1;
	address_[7] = stack_pointers_[is_supervisor_];

	// TODO: rest of status word: interrupt level, trace flag.
}
