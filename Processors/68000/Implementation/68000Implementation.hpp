//
//  68000Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#define get_status() \
	(	\
		(carry_flag_ 	? 0x0001 : 0x0000) |	\
		(overflow_flag_	? 0x0002 : 0x0000) |	\
		(zero_result_	? 0x0000 : 0x0004) |	\
		(negative_flag_	? 0x0008 : 0x0000) |	\
		(extend_flag_	? 0x0010 : 0x0000) |	\
		(interrupt_level_ << 8) |				\
		(trace_flag_ 	? 0x8000 : 0x0000) |	\
		(is_supervisor_ << 13)					\
	)

#define set_status(x) \
	carry_flag_			= (x) & 0x0001;	\
	overflow_flag_		= (x) & 0x0002;	\
	zero_result_		= ((x) & 0x0004) ^ 0x0004;	\
	negative_flag_		= (x) & 0x0008;	\
	extend_flag_		= (x) & 0x0010;	\
	interrupt_level_ 	= ((x) >> 8) & 7;	\
	trace_flag_			= (x) & 0x8000;	\
	set_is_supervisor(!!(((x) >> 13) & 1));

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
							std::cerr << "68000 Abilities exhausted; can't manage instruction " << std::hex << next_instruction << " from " << (program_counter_.full - 4) << std::endl;
							return;
						} else {
							std::cout << "Performing " << std::hex << next_instruction << " from " << (program_counter_.full - 4) << std::endl;
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
								/*
									ABCD adds the lowest bytes form the source and destination using BCD arithmetic,
									obeying the extend flag.
								*/
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
									zero_result_ |= result & 0xff;
									extend_flag_ = carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = ~(source ^ destination) & (destination ^ result) & 0x80;

									// Store the result.
									active_program_->destination->halves.low.halves.low = uint8_t(result);
								} break;

								// ADD and ADDA add two quantities, the latter sign extending and without setting any flags.
								case Operation::ADDb: {
									const uint8_t source = active_program_->source->halves.low.halves.low;
									const uint8_t destination = active_program_->destination->halves.low.halves.low;
									const int result = destination + source;

									zero_result_ = active_program_->destination->halves.low.halves.low = uint8_t(result);
									extend_flag_ = carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = ~(source ^ destination) & (destination ^ result) & 0x80;
								} break;

								case Operation::ADDw: {
									const uint16_t source = active_program_->source->halves.low.full;
									const uint16_t destination = active_program_->destination->halves.low.full;
									const int result = destination + source;

									zero_result_ = active_program_->destination->halves.low.full = uint16_t(result);
									extend_flag_ = carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = ~(source ^ destination) & (destination ^ result) & 0x8000;
								} break;

								case Operation::ADDl: {
									const uint32_t source = active_program_->source->full;
									const uint32_t destination = active_program_->destination->full;
									const uint64_t result = destination + source;

									zero_result_ = active_program_->destination->halves.low.full = uint32_t(result);
									extend_flag_ = carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = ~(source ^ destination) & (destination ^ result) & 0x80000000;
								} break;

								case Operation::ADDAw:
									active_program_->destination->full += int16_t(active_program_->source->halves.low.full);
								break;

								case Operation::ADDAl:
									active_program_->destination->full += active_program_->source->full;
								break;

								// BRA: alters the program counter, exclusively via the prefetch queue.
								case Operation::BRA: {
									const int8_t byte_offset = int8_t(prefetch_queue_.halves.high.halves.low);

									// A non-zero offset byte branches by just that amount; otherwise use the word
									// after as an offset. In both cases, treat as signed.
									if(byte_offset) {
										program_counter_.full = (program_counter_.full + byte_offset);
									} else {
										program_counter_.full += int16_t(prefetch_queue_.halves.low.full);
									}
									program_counter_.full -= 2;
								} break;

								// Two BTSTs: set the zero flag according to the value of the destination masked by
								// the bit named in the source modulo the operation size.
								case Operation::BTSTb:
									zero_result_ = active_program_->destination->full & (1 << (active_program_->source->full & 7));
								break;

								case Operation::BTSTl:
									zero_result_ = active_program_->destination->full & (1 << (active_program_->source->full & 31));
								break;

								// Bcc: evaluates the relevant condition and displacement size and then:
								//	if condition is false, schedules bus operations to get past this instruction;
								//	otherwise applies the offset and schedules bus operations to refill the prefetch queue.
								case Operation::Bcc: {
									// Grab the 8-bit offset.
									const int8_t byte_offset = int8_t(prefetch_queue_.halves.high.halves.low);

									// Test the conditional.
									const bool should_branch = evaluate_condition(prefetch_queue_.halves.high.halves.high);

									// Schedule something appropriate, by rewriting the program for this instruction temporarily.
									if(should_branch) {
										if(byte_offset) {
											program_counter_.full = (program_counter_.full + byte_offset);
										} else {
											program_counter_.full += int16_t(prefetch_queue_.halves.low.full);
										}
										program_counter_.full -= 2;
										active_micro_op_->bus_program = branch_taken_bus_steps_;
									} else {
										if(byte_offset) {
											active_micro_op_->bus_program = branch_byte_not_taken_bus_steps_;
										} else {
											active_micro_op_->bus_program = branch_word_not_taken_bus_steps_;
										}
									}
								} break;

								case Operation::DBcc: {
									// Decide what sort of DBcc this is.
									if(!evaluate_condition(prefetch_queue_.halves.high.halves.high)) {
										-- active_program_->source->halves.low.full;
										const auto target_program_counter = program_counter_.full + int16_t(prefetch_queue_.halves.low.full) - 2;

										if(active_program_->source->halves.low.full == 0xffff) {
											// This DBcc will be ignored as the counter has underflowed.
											// Schedule n np np np and continue. Assumed: the first np
											// is from where the branch would have been if taken?
											active_micro_op_->bus_program = dbcc_condition_false_no_branch_steps_;
											dbcc_false_address_ = target_program_counter;
										} else {
											// Take the branch. Change PC and schedule n np np.
											active_micro_op_->bus_program = dbcc_condition_false_branch_steps_;
											program_counter_.full = target_program_counter;
										}
									} else {
										// This DBcc will be ignored as the condition is true;
										// perform nn np np and continue.
										active_micro_op_->bus_program = dbcc_condition_true_steps_;
									}
								} break;

								/*
									CMP.b, CMP.l and CMP.w: sets the condition flags (other than extend) based on a subtraction
									of the source from the destination; the result of the subtraction is not stored.
								*/
								case Operation::CMPb: {
									const uint8_t source = active_program_->source->halves.low.halves.low;
									const uint8_t destination = active_program_->destination->halves.low.halves.low;
									const int result = destination - source;

									zero_result_ = result & 0xff;
									carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80;
								} break;

								case Operation::CMPw: {
									const uint16_t source = active_program_->source->halves.low.full;
									const uint16_t destination = active_program_->destination->halves.low.full;
									const int result = destination - source;

									zero_result_ = result & 0xffff;
									carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x8000;
								} break;

								case Operation::CMPl: {
									const uint32_t source = active_program_->source->full;
									const uint32_t destination = active_program_->destination->full;
									const uint64_t result = destination - source;

									zero_result_ = uint32_t(result);
									carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80000000;
								} break;

								// JMP: copies the source to the program counter.
								case Operation::JMP:
									program_counter_.full = active_program_->source->full;
								break;

								/*
									MOVE.b, MOVE.l and MOVE.w: move the least significant byte or word, or the entire long word,
									and set negative, zero, overflow and carry as appropriate.
								*/
								case Operation::MOVEb:
									zero_result_ = active_program_->destination->halves.low.halves.low = active_program_->source->halves.low.halves.low;
									negative_flag_ = zero_result_ & 0x80;
									overflow_flag_ = carry_flag_ = 0;
								break;

								case Operation::MOVEw:
									zero_result_ = active_program_->destination->halves.low.full = active_program_->source->halves.low.full;
									negative_flag_ = zero_result_ & 0x8000;
									overflow_flag_ = carry_flag_ = 0;
								break;

								case Operation::MOVEl:
									zero_result_ = active_program_->destination->full = active_program_->source->full;
									negative_flag_ = zero_result_ & 0x80000000;
									overflow_flag_ = carry_flag_ = 0;
								break;

								/*
									MOVE.q: a single byte is moved from the current instruction, and sign extended.
								*/
								case Operation::MOVEq:
									zero_result_ = active_program_->destination->full = prefetch_queue_.halves.high.halves.low;
									negative_flag_ = zero_result_ & 0x80;
									overflow_flag_ = carry_flag_ = 0;
									active_program_->destination->full |= negative_flag_ ? 0xffffff00 : 0;
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

								/*
									Status word moves.
								*/

								case Operation::MOVEtoSR:
									set_status(active_program_->source->full);
								break;

								case Operation::MOVEfromSR:
									active_program_->source->halves.low.full = get_status();
								break;

								/*
									The no-op.
								*/
								case Operation::None:
								break;

								/*
									SBCD subtracts the lowest byte of the source from that of the destination using
									BCD arithmetic, obeying the extend flag.
								*/
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
									zero_result_ |= result & 0xff;
									extend_flag_ = carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80;

									// Store the result.
									active_program_->destination->halves.low.halves.low = uint8_t(result);
								} break;

								case Operation::SUBb: {
									const uint8_t source = active_program_->source->halves.low.halves.low;
									const uint8_t destination = active_program_->destination->halves.low.halves.low;
									const int result = destination - source;

									zero_result_ = active_program_->destination->halves.low.halves.low = uint8_t(result);
									extend_flag_ = carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80;
								} break;

								case Operation::SUBw: {
									const uint16_t source = active_program_->source->halves.low.full;
									const uint16_t destination = active_program_->destination->halves.low.full;
									const int result = destination - source;

									zero_result_ = active_program_->destination->halves.low.full = uint16_t(result);
									extend_flag_ = carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x8000;
								} break;

								case Operation::SUBl: {
									const uint32_t source = active_program_->source->full;
									const uint32_t destination = active_program_->destination->full;
									const uint64_t result = destination - source;

									zero_result_ = active_program_->destination->halves.low.full = uint32_t(result);
									extend_flag_ = carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80000000;
								} break;

								case Operation::SUBAw:
									active_program_->destination->full -= int16_t(active_program_->source->halves.low.full);
								break;

								case Operation::SUBAl:
									active_program_->destination->full -= active_program_->source->full;
								break;

								/*
									Development period debugging.
								*/
								default:
									std::cerr << "Should do something with program operation " << int(active_program_->operation) << std::endl;
								break;
							}
						break;

						case int(MicroOp::Action::SetMoveFlagsb):
							zero_result_ = active_program_->source->halves.low.halves.low;
							negative_flag_ = zero_result_ & 0x80;
							overflow_flag_ = carry_flag_ = 0;
						break;

						case int(MicroOp::Action::SetMoveFlagsw):
							zero_result_ = active_program_->source->halves.low.full;
							negative_flag_ = zero_result_ & 0x8000;
							overflow_flag_ = carry_flag_ = 0;
						break;

						case int(MicroOp::Action::SetMoveFlagsl):
							zero_result_ = active_program_->source->full;
							negative_flag_ = zero_result_ & 0x80000000;
							overflow_flag_ = carry_flag_ = 0;
						break;

						// Increments and decrements.
#define Adjust(op, quantity)	\
	case int(op) | MicroOp::SourceMask:			active_program_->source_address->full += quantity;		break;	\
	case int(op) | MicroOp::DestinationMask:	active_program_->destination_address->full += quantity;	break;	\
	case int(op) | MicroOp::SourceMask | MicroOp::DestinationMask:	\
		active_program_->destination_address->full += quantity;	\
		active_program_->source_address->full += quantity;	\
	break;

						Adjust(MicroOp::Action::Decrement1, -1);
						Adjust(MicroOp::Action::Decrement2, -2);
						Adjust(MicroOp::Action::Decrement4, -4);
						Adjust(MicroOp::Action::Increment1, 1);
						Adjust(MicroOp::Action::Increment2, 2);
						Adjust(MicroOp::Action::Increment4, 4);

#undef Adjust

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

						// 16-bit offset addressing modes.

						case int(MicroOp::Action::CalcD16PC) | MicroOp::SourceMask:
							// The address the low part of the prefetch queue was read from was two bytes ago, hence
							// the subtraction of 2.
							effective_address_[0] = int16_t(prefetch_queue_.halves.low.full) + program_counter_.full - 2;
						break;

						case int(MicroOp::Action::CalcD16PC) | MicroOp::DestinationMask:
							effective_address_[1] = int16_t(prefetch_queue_.halves.low.full) + program_counter_.full - 2;
						break;

						case int(MicroOp::Action::CalcD16PC) | MicroOp::SourceMask | MicroOp::DestinationMask:
							// Similar logic applies here to above, but the high part of the prefetch queue was four bytes
							// ago rather than merely two.
							effective_address_[0] = int16_t(prefetch_queue_.halves.high.full) + program_counter_.full - 4;
							effective_address_[1] = int16_t(prefetch_queue_.halves.low.full) + program_counter_.full - 2;
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

#define CalculateD8AnXn(data, source, target)	{\
	const auto register_index = (data.full >> 12) & 7;	\
	const RegisterPair32 &displacement = (data.full & 0x8000) ? address_[register_index] : data_[register_index];	\
	target.full = int8_t(data.halves.low) + source;	\
\
	if(data.full & 0x800) {	\
		target.full += displacement.halves.low.full;	\
	} else {	\
		target.full += displacement.full;	\
	}	\
}
						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::SourceMask: {
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->source->full, effective_address_[0]);
						} break;

						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->destination->full, effective_address_[1]);
						} break;

						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::SourceMask | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.high, active_program_->source->full, effective_address_[0]);
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->destination->full, effective_address_[1]);
						} break;

						case int(MicroOp::Action::CalcD8PCXn) | MicroOp::SourceMask: {
							CalculateD8AnXn(prefetch_queue_.halves.low, program_counter_.full - 2, effective_address_[0]);
						} break;

						case int(MicroOp::Action::CalcD8PCXn) | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.low, program_counter_.full - 2, effective_address_[1]);
						} break;

						case int(MicroOp::Action::CalcD8PCXn) | MicroOp::SourceMask | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.high, program_counter_.full - 4, effective_address_[0]);
							CalculateD8AnXn(prefetch_queue_.halves.low, program_counter_.full - 2, effective_address_[1]);
						} break;

#undef CalculateD8AnXn

						case int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::SourceMask:
							// Assumption: this will be assembling right at the start of the instruction.
							effective_address_[0] = prefetch_queue_.halves.low.full;
						break;

						case int(MicroOp::Action::AssembleWordAddressFromPrefetch) | MicroOp::DestinationMask:
							effective_address_[1] = prefetch_queue_.halves.low.full;
						break;

						case int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::SourceMask:
							effective_address_[0] = prefetch_queue_.full;
						break;

						case int(MicroOp::Action::AssembleLongWordAddressFromPrefetch) | MicroOp::DestinationMask:
							effective_address_[1] = prefetch_queue_.full;
						break;

						case int(MicroOp::Action::AssembleWordDataFromPrefetch) | MicroOp::SourceMask:
							// Assumption: this will be assembling right at the start of the instruction.
							source_bus_data_[0] = prefetch_queue_.halves.low.full;
						break;

						case int(MicroOp::Action::AssembleWordDataFromPrefetch) | MicroOp::DestinationMask:
							destination_bus_data_[0] = prefetch_queue_.halves.low.full;
						break;

						case int(MicroOp::Action::AssembleLongWordDataFromPrefetch) | MicroOp::SourceMask:
							source_bus_data_[0] = prefetch_queue_.full;
						break;

						case int(MicroOp::Action::AssembleLongWordDataFromPrefetch) | MicroOp::DestinationMask:
							destination_bus_data_[0] = prefetch_queue_.full;
						break;

						case int(MicroOp::Action::CopyToEffectiveAddress) | MicroOp::SourceMask:
							effective_address_[0] = *active_program_->source_address;
						break;

						case int(MicroOp::Action::CopyToEffectiveAddress) | MicroOp::DestinationMask:
							effective_address_[1] = *active_program_->destination_address;
						break;

						case int(MicroOp::Action::CopyToEffectiveAddress) | MicroOp::SourceMask | MicroOp::DestinationMask:
							effective_address_[0] = *active_program_->source_address;
							effective_address_[1] = *active_program_->destination_address;
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

				case BusStep::Action::IncrementEffectiveAddress0:	effective_address_[0].full += 2;	break;
				case BusStep::Action::IncrementEffectiveAddress1:	effective_address_[1].full += 2;	break;
				case BusStep::Action::DecrementEffectiveAddress0:	effective_address_[0].full -= 2;	break;
				case BusStep::Action::DecrementEffectiveAddress1:	effective_address_[1].full -= 2;	break;
				case BusStep::Action::IncrementProgramCounter:		program_counter_.full += 2;			break;

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

	state.status = get_status();

	return state;
}

template <class T, bool dtack_is_implicit> void Processor<T, dtack_is_implicit>::set_state(const ProcessorState &state) {
	memcpy(data_, state.data, sizeof(state.data));
	memcpy(address_, state.address, sizeof(state.address));
	stack_pointers_[0].full = state.user_stack_pointer;
	stack_pointers_[1].full = state.supervisor_stack_pointer;

	set_status(state.status);

	address_[7] = stack_pointers_[is_supervisor_];
}

#undef get_status
#undef set_status
