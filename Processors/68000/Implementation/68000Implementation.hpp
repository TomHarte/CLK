//
//  68000Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#define get_ccr() \
	(	\
		(carry_flag_ 	? 0x0001 : 0x0000) |	\
		(overflow_flag_	? 0x0002 : 0x0000) |	\
		(zero_result_	? 0x0000 : 0x0004) |	\
		(negative_flag_	? 0x0008 : 0x0000) |	\
		(extend_flag_	? 0x0010 : 0x0000)		\
	)

#define get_status() \
	(	\
		get_ccr() |	\
		(interrupt_level_ << 8) |				\
		(trace_flag_ 	? 0x8000 : 0x0000) |	\
		(is_supervisor_ << 13)					\
	)

#define set_ccr(x) \
	carry_flag_			= (x) & 0x0001;	\
	overflow_flag_		= (x) & 0x0002;	\
	zero_result_		= ((x) & 0x0004) ^ 0x0004;	\
	negative_flag_		= (x) & 0x0008;	\
	extend_flag_		= (x) & 0x0010;

#define set_status(x) \
	set_ccr(x)	\
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
						if(address_[5].full > 0x3fff0 || program_counter_.full < 0x16a) {
							std::cout << std::setfill('0');
							std::cout << (extend_flag_ ? 'x' : '-') << (negative_flag_ ? 'n' : '-') << (zero_result_ ? '-' : 'z');
							std::cout << (overflow_flag_ ? 'v' : '-') << (carry_flag_ ? 'c' : '-') << '\t';
							for(int c = 0; c < 8; ++ c) std::cout << "d" << c << ":" << std::setw(8) << data_[c].full << " ";
							for(int c = 0; c < 8; ++ c) std::cout << "a" << c << ":" << std::setw(8) << address_[c].full << " ";
						}
						std::cout << '\n';

						decoded_instruction_ = prefetch_queue_.halves.high.full;
						if(!instructions[decoded_instruction_].micro_operations) {
							// TODO: once all instructions are implemnted, this should be an instruction error.
							std::cerr << "68000 Abilities exhausted; can't manage instruction " << std::hex << decoded_instruction_ << " from " << (program_counter_.full - 4) << std::endl;
							return;
						} else {
							std::cout << std::hex << (program_counter_.full - 4) << ": " << std::setw(4) << decoded_instruction_ << '\t';
						}

						active_program_ = &instructions[decoded_instruction_];
						active_micro_op_ = active_program_->micro_operations;
					}

					auto bus_program = active_micro_op_->bus_program;
					switch(active_micro_op_->action) {
						default:
							std::cerr << "Unhandled 68000 micro op action " << std::hex << active_micro_op_->action << std::endl;
						break;

						case int(MicroOp::Action::None): break;

						case int(MicroOp::Action::PerformOperation):
#define sub_overflow() (source ^ destination) & (destination ^ result)
#define add_overflow() ~(source ^ destination) & (destination ^ result)
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
									overflow_flag_ = add_overflow() & 0x80;

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
									overflow_flag_ = add_overflow() & 0x80;
								} break;

								case Operation::ADDw: {
									const uint16_t source = active_program_->source->halves.low.full;
									const uint16_t destination = active_program_->destination->halves.low.full;
									const int result = destination + source;

									zero_result_ = active_program_->destination->halves.low.full = uint16_t(result);
									extend_flag_ = carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = add_overflow() & 0x8000;
								} break;

								case Operation::ADDl: {
									const uint32_t source = active_program_->source->full;
									const uint32_t destination = active_program_->destination->full;
									const uint64_t result = destination + source;

									zero_result_ = active_program_->destination->full = uint32_t(result);
									extend_flag_ = carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = add_overflow() & 0x80000000;
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

								case Operation::BCLRb:
									zero_result_ = active_program_->destination->full & (1 << (active_program_->source->full & 7));
									active_program_->destination->full &= ~(1 << (active_program_->source->full & 7));
								break;

								case Operation::BCLRl:
									zero_result_ = active_program_->destination->full & (1 << (active_program_->source->full & 31));
									active_program_->destination->full &= ~(1 << (active_program_->source->full & 31));

									// Clearing in the top word requires an extra four cycles.
									active_step_->microcycle.length = HalfCycles(8 + ((active_program_->source->full & 31) / 16) * 4);
								break;

								// Bcc: ordinarily evaluates the relevant condition and displacement size and then:
								//	if condition is false, schedules bus operations to get past this instruction;
								//	otherwise applies the offset and schedules bus operations to refill the prefetch queue.
								//
								// Special case: the condition code is 1, which is ordinarily false. In that case this
								// is the trailing step of a BSR.
								case Operation::Bcc: {
									// Grab the 8-bit offset.
									const int8_t byte_offset = int8_t(prefetch_queue_.halves.high.halves.low);

									// Check whether this is secretly BSR.
									const bool is_bsr = ((decoded_instruction_ >> 8) & 0xf) == 1;

									// Test the conditional, treating 'false' as true.
									const bool should_branch = is_bsr || evaluate_condition(prefetch_queue_.halves.high.halves.high);

									// Schedule something appropriate, by rewriting the program for this instruction temporarily.
									if(should_branch) {
										if(byte_offset) {
											program_counter_.full = (program_counter_.full + byte_offset);
										} else {
											program_counter_.full += int16_t(prefetch_queue_.halves.low.full);
										}
										program_counter_.full -= 2;
										bus_program = is_bsr ? bsr_bus_steps_ : branch_taken_bus_steps_;
									} else {
										if(byte_offset) {
									 		bus_program = branch_byte_not_taken_bus_steps_;
										} else {
											bus_program = branch_word_not_taken_bus_steps_;
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
											bus_program = dbcc_condition_false_no_branch_steps_;
											dbcc_false_address_ = target_program_counter;
										} else {
											// Take the branch. Change PC and schedule n np np.
											bus_program = dbcc_condition_false_branch_steps_;
											program_counter_.full = target_program_counter;
										}
									} else {
										// This DBcc will be ignored as the condition is true;
										// perform nn np np and continue.
										bus_program = dbcc_condition_true_steps_;
									}
								} break;

								case Operation::Scc: {
									active_program_->destination->halves.low.halves.low =
										evaluate_condition(prefetch_queue_.halves.high.halves.high) ? 0xff : 0x00;
								} break;

								/*
									CLRs: store 0 to the destination, set the zero flag, and clear
									negative, overflow and carry.
								*/
								case Operation::CLRb:
									active_program_->destination->halves.low.halves.low = 0;
									negative_flag_ = overflow_flag_ = carry_flag_ = zero_result_ = 0;
								break;

								case Operation::CLRw:
									active_program_->destination->halves.low.full = 0;
									negative_flag_ = overflow_flag_ = carry_flag_ = zero_result_ = 0;
								break;

								case Operation::CLRl:
									active_program_->destination->full = 0;
									negative_flag_ = overflow_flag_ = carry_flag_ = zero_result_ = 0;
								break;

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
									overflow_flag_ = sub_overflow() & 0x80;
								} break;

								case Operation::CMPw: {
									const uint16_t source = active_program_->source->halves.low.full;
									const uint16_t destination = active_program_->destination->halves.low.full;
									const int result = destination - source;

									zero_result_ = result & 0xffff;
									carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = sub_overflow() & 0x8000;
								} break;

								case Operation::CMPl: {
									const uint32_t source = active_program_->source->full;
									const uint32_t destination = active_program_->destination->full;
									const uint64_t result = destination - source;

									zero_result_ = uint32_t(result);
									carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = sub_overflow() & 0x80000000;
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

								case Operation::MOVEtoCCR:
									set_ccr(active_program_->source->full);
								break;

								/*
									MOVEM: multi-word moves.
								*/

#define setup_movem(words_per_reg, base)								\
	/* Count the number of long words to move.	 */						\
	size_t total_to_move = 0;											\
	auto mask = next_word_;												\
	while(mask) {														\
		total_to_move += mask&1;										\
		mask >>= 1;														\
	}																	\
																		\
	/* Twice that many words plus one will need to be moved */			\
	bus_program = base + (64 - total_to_move*words_per_reg)*2;			\
																		\
	/* Fill in the proper addresses and targets. */						\
	const auto mode = (decoded_instruction_ >> 3) & 7;					\
	uint32_t start_address;												\
	if(mode <= 4) {														\
		start_address = active_program_->destination_address->full;		\
	} else {															\
		start_address = effective_address_[1].full;						\
	}																	\
																		\
	auto step = bus_program;											\
	uint32_t *address_storage = movem_addresses_;						\
	mask = next_word_;													\
	int offset = 0;

#define inc_action(x, v) x += v
#define dec_action(x, v) x -= v

#define write_address_sequence_long(action, l)														\
	while(mask) {																					\
		if(mask&1) {																				\
			address_storage[0] = start_address;														\
			action(start_address, 2);																\
			address_storage[1] = start_address;														\
			action(start_address, 2);																\
																									\
			step[0].microcycle.address = step[1].microcycle.address = address_storage;				\
			step[2].microcycle.address = step[3].microcycle.address = address_storage + 1;			\
																									\
			const auto target = (offset > 7) ? &address_[offset&7] : &data_[offset];				\
			step[(l^2)].microcycle.value = step[(l^2)+1].microcycle.value = &target->halves.high;	\
			step[l].microcycle.value = step[l+1].microcycle.value = &target->halves.low;			\
																									\
			address_storage += 2;																	\
			step += 4;																				\
		}																							\
		mask >>= 1;																					\
		action(offset, 1);																			\
	}

#define write_address_sequence_word(action)												\
	while(mask) {																		\
		if(mask&1) {																	\
			address_storage[0] = start_address;											\
			action(start_address, 2);													\
																						\
			step[0].microcycle.address = step[1].microcycle.address = address_storage;	\
																						\
			const auto target = (offset > 7) ? &address_[offset&7] : &data_[offset];	\
			step[0].microcycle.value = step[1].microcycle.value = &target->halves.low;	\
																						\
			++ address_storage;															\
			step += 2;																	\
		}																				\
		mask >>= 1;																		\
		action(offset, 1);																\
	}

								case Operation::MOVEMtoRl: {
									setup_movem(2, movem_reads_steps_);

									// Everything for move to registers is based on an incrementing
									// address; per M68000PRM:
									//
									// "[If using the postincrement addressing mode then] the incremented address
									// register contains the address of the last operand loaded plus the operand length.
									// If the addressing register is also loaded from memory, the memory value is ignored
									// and the register is written with the postincremented effective address."
									//
									// The latter part is dealt with by MicroOp::Action::MOVEMtoRComplete, which also
									// does any necessary sign extension.
									write_address_sequence_long(inc_action, 0);

									// MOVEM to R always reads one word too many.
									address_storage[0] = start_address;
									step[0].microcycle.address = step[1].microcycle.address = address_storage;
									step[0].microcycle.value = step[1].microcycle.value = &movem_spare_value_;
									movem_final_address_ = start_address;
								} break;

								case Operation::MOVEMtoRw: {
									setup_movem(1, movem_reads_steps_);
									write_address_sequence_word(inc_action);

									// MOVEM to R always reads one word too many.
									address_storage[0] = start_address;
									step[0].microcycle.address = step[1].microcycle.address = address_storage;
									step[0].microcycle.value = step[1].microcycle.value = &movem_spare_value_;
									movem_final_address_ = start_address;
								} break;

								case Operation::MOVEMtoMl: {
									setup_movem(2, movem_writes_steps_);

									// MOVEM to M counts downwards and enumerates the registers in reverse order
									// if subject to the predecrementing mode; otherwise it counts upwards and
									// operates exactly as does MOVEM to R.
									//
									// Note also: "The MC68000 and MC68010 write the initial register value
									// (not decremented) [when writing a register that is providing
									// pre-decrementing addressing]."
									//
									// Hence the decrementing register (if any) is updated
									// by MicroOp::Action::MOVEMtoMComplete.
									if(mode == 4) {
										offset = 15;
										start_address -= 2;
										write_address_sequence_long(dec_action, 2);
										movem_final_address_ = start_address;
									} else {
										write_address_sequence_long(inc_action, 0);
									}
								} break;

								case Operation::MOVEMtoMw: {
									setup_movem(1, movem_writes_steps_);

									if(mode == 4) {
										offset = 15;
										start_address -= 2;
										write_address_sequence_word(dec_action);
									} else {
										write_address_sequence_word(inc_action);
									}
								} break;

#undef setup_movem
#undef write_address_sequence_long
#undef write_address_sequence_word
#undef inc_action
#undef dec_action

								/*
									NEGs: negatives the destination, setting the zero,
									negative, overflow and carry flags appropriate, and extend.
								*/
								case Operation::NEGb: {
									const int source = 0;
									const int destination = active_program_->destination->halves.low.halves.low;
									const int result = source - destination;
									active_program_->destination->halves.low.halves.low = result;

									zero_result_ = result & 0xff;
									extend_flag_ = carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = sub_overflow() & 0x80;
								} break;

								case Operation::NEGw: {
									const int source = 0;
									const int destination = active_program_->destination->halves.low.full;
									const int result = source - destination;
									active_program_->destination->halves.low.full = result;

									zero_result_ = result & 0xffff;
									extend_flag_ = carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = sub_overflow() & 0x8000;
								} break;

								case Operation::NEGl: {
									const int source = 0;
									const int destination = active_program_->destination->full;
									int64_t result = source - destination;
									active_program_->destination->full = uint32_t(result);

									zero_result_ = uint_fast32_t(result);
									extend_flag_ = carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = sub_overflow() & 0x80000000;
								} break;

								/*
									NEGXs: NEG, with extend.
								*/
								case Operation::NEGXb: {
									const int source = 0;
									const int destination = active_program_->destination->halves.low.halves.low;
									const int result = source - destination - (extend_flag_ ? 1 : 0);
									active_program_->destination->halves.low.halves.low = result;

									zero_result_ = result & 0xff;
									extend_flag_ = carry_flag_ = result & ~0xff;
									negative_flag_ = result & 0x80;
									overflow_flag_ = sub_overflow() & 0x80;
								} break;

								case Operation::NEGXw: {
									const int source = 0;
									const int destination = active_program_->destination->halves.low.full;
									const int result = source - destination - (extend_flag_ ? 1 : 0);
									active_program_->destination->halves.low.full = result;

									zero_result_ = result & 0xffff;
									extend_flag_ = carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = sub_overflow() & 0x8000;
								} break;

								case Operation::NEGXl: {
									const int source = 0;
									const int destination = active_program_->destination->full;
									int64_t result = source - destination - (extend_flag_ ? 1 : 0);
									active_program_->destination->full = uint32_t(result);

									zero_result_ = uint_fast32_t(result);
									extend_flag_ = carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = sub_overflow() & 0x80000000;
								} break;

								/*
									The no-op.
								*/
								case Operation::None:
								break;

								// NOTs: take the logical inverse, affecting the negative and zero flags.

								case Operation::NOTb:
									active_program_->destination->halves.low.halves.low ^= 0xff;
									zero_result_ = active_program_->destination->halves.low.halves.low;
									negative_flag_ = zero_result_ & 0x80;
								break;

								case Operation::NOTw:
									active_program_->destination->halves.low.full ^= 0xffff;
									zero_result_ = active_program_->destination->halves.low.full;
									negative_flag_ = zero_result_ & 0x8000;
								break;

								case Operation::NOTl:
									active_program_->destination->full ^= 0xffffffff;
									zero_result_ = active_program_->destination->full;
									negative_flag_ = zero_result_ & 0x80000000;
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
									overflow_flag_ = sub_overflow() & 0x80;

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
									overflow_flag_ = sub_overflow() & 0x80;
								} break;

								case Operation::SUBw: {
									const uint16_t source = active_program_->source->halves.low.full;
									const uint16_t destination = active_program_->destination->halves.low.full;
									const int result = destination - source;

									zero_result_ = active_program_->destination->halves.low.full = uint16_t(result);
									extend_flag_ = carry_flag_ = result & ~0xffff;
									negative_flag_ = result & 0x8000;
									overflow_flag_ = sub_overflow() & 0x8000;
								} break;

								case Operation::SUBl: {
									const uint32_t source = active_program_->source->full;
									const uint32_t destination = active_program_->destination->full;
									const uint64_t result = destination - source;

									zero_result_ = active_program_->destination->full = uint32_t(result);
									extend_flag_ = carry_flag_ = result >> 32;
									negative_flag_ = result & 0x80000000;
									overflow_flag_ = sub_overflow() & 0x80000000;
								} break;

								case Operation::SUBAw:
									active_program_->destination->full -= int16_t(active_program_->source->halves.low.full);
								break;

								case Operation::SUBAl:
									active_program_->destination->full -= active_program_->source->full;
								break;

								/*
									Shifts and rotates.
								*/
#define set_neg_zero_overflow(v, m)	\
	zero_result_ = (v);	\
	negative_flag_ = zero_result_ & (m);	\
	overflow_flag_ = (value ^ zero_result_) & (m);

#define set_flags(v, m, t)	\
	set_neg_zero_overflow(v, m)	\
	extend_flag_ = carry_flag_ = value & (t);

#define decode_shift_count()	\
	int shift_count = (decoded_instruction_ & 32) ? data_[(decoded_instruction_ >> 9) & 7].full&63 : ( ((decoded_instruction_ >> 9)&7) ? ((decoded_instruction_ >> 9)&7) : 8) ;	\
	active_step_->microcycle.length = HalfCycles(4 * shift_count);

#define set_flags_b(t) set_flags(active_program_->destination->halves.low.halves.low, 0x80, t)
#define set_flags_w(t) set_flags(active_program_->destination->halves.low.full, 0x8000, t)
#define set_flags_l(t) set_flags(active_program_->destination->full, 0x80000000, t)

#define shift_op(name, op, mb, mw, ml)	\
	case Operation::name##b: {	\
		decode_shift_count();	\
		const auto value = active_program_->destination->halves.low.halves.low;	\
		op(active_program_->destination->halves.low.halves.low, shift_count, 0xff, 7);	\
		set_flags_b(mb);	\
	} break;	\
\
	case Operation::name##w: {	\
		decode_shift_count();	\
		const auto value = active_program_->destination->halves.low.full;	\
		op(active_program_->destination->halves.low.full, shift_count, 0xffff, 15);	\
		set_flags_w(mw);	\
	} break;	\
\
	case Operation::name##l: {	\
		decode_shift_count();	\
		const auto value = active_program_->destination->full;	\
		op(active_program_->destination->full, shift_count, 0xffffffff, 31);	\
		set_flags_l(ml);	\
	} break;	\
\
	case Operation::name##m: {	\
		const auto value = active_program_->destination->halves.low.full;	\
		op(active_program_->destination->halves.low.full, 1, 0xffff, 15);	\
		set_flags_w(mw);	\
	} break;

#define lsl(x, c, m, d) x <<= c
#define lsr(x, c, m, d) x >>= c
#define asl(x, c, m, d) x <<= c
#define asr(x, c, m, d) x = (x >> c) | (((value >> d) & 1) *  (m ^ (m >> c)))

// TODO: carry/extend is incorrect for shifts of greater than one digit.

								shift_op(LSL, lsl, 0x80, 0x8000, 0x80000000);
								shift_op(ASL, lsl, 0x80, 0x8000, 0x80000000);
								shift_op(LSR, lsr, 0x01, 0x0001, 0x00000001);
								shift_op(ASR, asr, 0x01, 0x0001, 0x00000001);

#undef set_flags
#define set_flags(v, m, t)	\
	zero_result_ = v;	\
	negative_flag_ = zero_result_ & (m);	\
	overflow_flag_ = 0;	\
	carry_flag_ = value & (t);

#define rol(destination, size)	{ \
		decode_shift_count();	\
		const auto value = destination;	\
		\
		if(!shift_count) {	\
			carry_flag_ = 0;	\
		} else {	\
			shift_count &= (size - 1);	\
			destination =	\
				(value << shift_count) |	\
				(value >> (size - shift_count));	\
			carry_flag_ = destination & 1;	\
		}	\
		\
		set_neg_zero_overflow(destination, 1 << (size - 1));	\
	}

								case Operation::ROLm: {
									const auto value = active_program_->destination->halves.low.full;
									active_program_->destination->halves.low.full = (value << 1) | (value >> 15);
									carry_flag_ = active_program_->destination->halves.low.full & 1;
									set_neg_zero_overflow(active_program_->destination->halves.low.full, 0x8000);
								} break;
								case Operation::ROLb: rol(active_program_->destination->halves.low.halves.low, 8);	break;
								case Operation::ROLw: rol(active_program_->destination->halves.low.full, 16); 		break;
								case Operation::ROLl: rol(active_program_->destination->full, 32); 					break;

#undef rol

#define ror(destination, size)	{ \
		decode_shift_count();	\
		const auto value = destination;	\
		\
		if(!shift_count) {	\
			carry_flag_ = 0;	\
		} else {	\
			shift_count &= (size - 1);	\
			destination =	\
				(value >> shift_count) |	\
				(value << (size - shift_count));	\
			carry_flag_ = destination & (1 << (size - 1));	\
		}	\
		\
		set_neg_zero_overflow(destination, 1 << (size - 1));	\
	}

								case Operation::RORm: {
									const auto value = active_program_->destination->halves.low.full;
									active_program_->destination->halves.low.full = (value >> 1) | (value << 15);
									carry_flag_ = active_program_->destination->halves.low.full & 0x8000;
									set_neg_zero_overflow(active_program_->destination->halves.low.full, 0x8000);
								} break;
								case Operation::RORb: ror(active_program_->destination->halves.low.halves.low, 8);	break;
								case Operation::RORw: ror(active_program_->destination->halves.low.full, 16); 		break;
								case Operation::RORl: ror(active_program_->destination->full, 32); 					break;

#undef ror

#define roxl(destination, size)	{ \
	decode_shift_count();	\
	const auto value = destination;	\
\
	if(!shift_count) {	\
		carry_flag_ = extend_flag_;	\
	} else {	\
		shift_count %= (size + 1);	\
		destination =	\
			(value << shift_count) |	\
			(value >> (size + 1 - shift_count)) |	\
			((extend_flag_ ? (1 << (size - 1)) : 0) >> (size - shift_count));	\
		carry_flag_ = extend_flag_ = destination & 1;	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

								case Operation::ROXLm: {
									const auto value = active_program_->destination->halves.low.full;
									active_program_->destination->halves.low.full = (value << 1) | (extend_flag_ ? 0x0001 : 0x0000);
									extend_flag_ = value & 0x8000;
									set_flags_w(0x8000);
								} break;
								case Operation::ROXLb: roxl(active_program_->destination->halves.low.halves.low, 8);	break;
								case Operation::ROXLw: roxl(active_program_->destination->halves.low.full, 16); 		break;
								case Operation::ROXLl: roxl(active_program_->destination->full, 32); 					break;

#undef roxl

#define roxr(destination, size)	{ \
	decode_shift_count();	\
	const auto value = destination;	\
\
	if(!shift_count) {	\
		carry_flag_ = extend_flag_;	\
	} else {	\
		shift_count %= (size + 1);	\
		destination =	\
			(value >> shift_count) |	\
			(value << (size + 1 - shift_count)) |	\
			((extend_flag_ ? 1 : 0) << (size - shift_count));	\
		carry_flag_ = extend_flag_ = destination & 1;	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

								case Operation::ROXRm: {
									const auto value = active_program_->destination->halves.low.full;
									active_program_->destination->halves.low.full = (value >> 1) | (extend_flag_ ? 0x8000 : 0x0000);
									extend_flag_ = value & 0x0001;
									set_flags_w(0x0001);
								} break;
								case Operation::ROXRb: roxr(active_program_->destination->halves.low.halves.low, 8);	break;
								case Operation::ROXRw: roxr(active_program_->destination->halves.low.full, 16); 		break;
								case Operation::ROXRl: roxr(active_program_->destination->full, 32); 					break;

#undef roxr

#undef set_flags
#undef decode_shift_count
#undef set_flags_b
#undef set_flags_w
#undef set_flags_l
#undef set_neg_zero_overflow

								/*
									TSTs: compare to zero.
								*/

								case Operation::TSTb:
									carry_flag_ = overflow_flag_ = 0;
									zero_result_ = active_program_->source->halves.low.halves.low;
									negative_flag_ = zero_result_ & 0x80;
								break;

								case Operation::TSTw:
									carry_flag_ = overflow_flag_ = 0;
									zero_result_ = active_program_->source->halves.low.full;
									negative_flag_ = zero_result_ & 0x8000;
								break;

								case Operation::TSTl:
									carry_flag_ = overflow_flag_ = 0;
									zero_result_ = active_program_->source->full;
									negative_flag_ = zero_result_ & 0x80000000;
								break;

								/*
									Development period debugging.
								*/
								default:
									std::cerr << "Should do something with program operation " << int(active_program_->operation) << std::endl;
								break;
							}
#undef sub_overflow
#undef add_overflow
						break;

						case int(MicroOp::Action::MOVEMtoRComplete): {
							// If this was a word-sized move, perform sign extension.
							if(active_program_->operation == Operation::MOVEMtoRw) {
								auto mask = next_word_;
								int offset = 0;
								while(mask) {
									if(mask&1) {
										const auto target = (offset > 7) ? &address_[offset&7] : &data_[offset];
										target->halves.high.full = (target->halves.low.full & 0x8000) ? 0xffff : 0x0000;
									}
									mask >>= 1;
									++offset;
								}
							}

							// If the post-increment mode was used, overwrite the source register.
							const auto mode = (decoded_instruction_ >> 3) & 7;
							if(mode == 3) {
								const auto reg = decoded_instruction_ & 7;
								address_[reg] = movem_final_address_;
							}
						} break;

						case int(MicroOp::Action::MOVEMtoMComplete): {
							const auto mode = (decoded_instruction_ >> 3) & 7;
							if(mode == 4) {
								const auto reg = decoded_instruction_ & 7;
								address_[reg] = movem_final_address_;
							}
						} break;

						case int(MicroOp::Action::PrepareJSR):
							destination_bus_data_[0] = program_counter_;
							address_[7].full -= 4;
							effective_address_[1].full = address_[7].full;
						break;

						case int(MicroOp::Action::PrepareRTS):
							effective_address_[0].full = address_[7].full;
							address_[7].full += 4;
						break;

						case int(MicroOp::Action::CopyNextWord):
							next_word_ = prefetch_queue_.halves.low.full;
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
							effective_address_[0] = int16_t(prefetch_queue_.halves.low.full) + active_program_->source_address->full;
						break;

						case int(MicroOp::Action::CalcD16An) | MicroOp::DestinationMask:
							effective_address_[1] = int16_t(prefetch_queue_.halves.low.full) + active_program_->destination_address->full;
						break;

						case int(MicroOp::Action::CalcD16An) | MicroOp::SourceMask | MicroOp::DestinationMask:
							effective_address_[0] = int16_t(prefetch_queue_.halves.high.full) + active_program_->source_address->full;
							effective_address_[1] = int16_t(prefetch_queue_.halves.low.full) + active_program_->destination_address->full;
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
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->source_address->full, effective_address_[0]);
						} break;

						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->destination_address->full, effective_address_[1]);
						} break;

						case int(MicroOp::Action::CalcD8AnXn) | MicroOp::SourceMask | MicroOp::DestinationMask: {
							CalculateD8AnXn(prefetch_queue_.halves.high, active_program_->source_address->full, effective_address_[0]);
							CalculateD8AnXn(prefetch_queue_.halves.low, active_program_->destination_address->full, effective_address_[1]);
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
						active_step_ = bus_program;
						if(!active_step_->is_terminal())
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
#undef set_ccr
#undef get_ccr
