//
//  PerformImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_PerformImplementation_h
#define InstructionSets_M68k_PerformImplementation_h

#include <cassert>
#include <cmath>

namespace InstructionSet {
namespace M68k {

#define u_extend16(x)	uint32_t(int16_t(x))
#define u_extend8(x)	uint32_t(int8_t(x))
#define s_extend16(x)	int32_t(int16_t(x))
#define s_extend8(x)	int32_t(int8_t(x))

#define convert_to_bit_count_16(x)	\
	x = ((x & 0xaaaa) >> 1) + (x & 0x5555);	\
	x = ((x & 0xcccc) >> 2) + (x & 0x3333);	\
	x = ((x & 0xf0f0) >> 4) + (x & 0x0f0f);	\
	x = ((x & 0xff00) >> 8) + (x & 0x00ff);

// TODO: decisions outstanding:
//
//	(1)	for DBcc, etc, should this receive the opcode in order to decode the conditional,
//		or is it a design error to do any _decoding_ here rather than in the decoder? If
//		the latter, should the internal cc operations all treat conditional as another
//		Q-style operand?
//
//	(2)	should I reintroduce the BTSTl/BTSTw-type distinctions, given that the only way to
//		determine them otherwise is by operand types and I'm hoping to treat data into
//		here as a black box?
//
//	(3)	to what extent, if any, should this function have responsibility for a MOVEM, MOVEP,
//		etc? This factoring is inteded to separate the bus interface from internal logic so
//		is there much to do here in any case? As currently drafted, something else will
//		already have had to check the operation and cue up data.
//
//	(4)	related to that, should the flow controller actually offer effective address calculation
//		and load/stores, along with a flag indicating whether to stop after loads? By the
//		magic of templates that'd avoid having a correlated sequencer — for the non-bus-accurate
//		68ks the loads and stores could be performed immediately, for the accurate they could
//		be enqueued, then performed, then a second call to perform that now has the data loaded
//		could be performed.

template <
	Operation operation,
	Model model,
	typename FlowController
> void perform(CPU::SlicedInt32 &src, CPU::SlicedInt32 &dest, Status &status, FlowController &flow_controller) {

#define sub_overflow() ((result ^ destination) & (destination ^ source))
#define add_overflow() ((result ^ destination) & ~(destination ^ source))
	switch(operation) {
		/*
			ABCD adds the lowest bytes form the source and destination using BCD arithmetic,
			obeying the extend flag.
		*/
		case Operation::ABCD: {
			// Pull out the two halves, for simplicity.
			const uint8_t source = src.b;
			const uint8_t destination = dest.b;

			// Perform the BCD add by evaluating the two nibbles separately.
			const int unadjusted_result = destination + source + (status.extend_flag_ ? 1 : 0);
			int result = (destination & 0xf) + (source & 0xf) + (status.extend_flag_ ? 1 : 0);
			if(result > 0x09) result += 0x06;
			result += (destination & 0xf0) + (source & 0xf0);
			if(result > 0x99) result += 0x60;

			// Set all flags essentially as if this were normal addition.
			status.zero_result_ |= result & 0xff;
			status.extend_flag_ = status.carry_flag_ = uint_fast32_t(result & ~0xff);
			status.negative_flag_ = result & 0x80;
			status.overflow_flag_ = ~unadjusted_result & result & 0x80;

			// Store the result.
			dest.b = uint8_t(result);
		} break;

#define addop(a, b, x) 	a + b + (x ? 1 : 0)
#define subop(a, b, x) 	a - b - (x ? 1 : 0)
#define z_set(a, b)		a = b
#define z_or(a, b)		a |= b

#define addsubb(a, b, op, overflow, x, zero_op)	\
	const int source = a;	\
	const int destination = b;	\
	const auto result = op(destination, source, x);	\
	\
	b = uint8_t(result);	\
	zero_op(status.zero_result_, b);	\
	status.extend_flag_ = status.carry_flag_ = uint_fast32_t(result & ~0xff);	\
	status.negative_flag_ = result & 0x80;	\
	status.overflow_flag_ = overflow() & 0x80;

#define addsubw(a, b, op, overflow, x, zero_op)	\
	const int source = a;	\
	const int destination = b;	\
	const auto result = op(destination, source, x);	\
	\
	b = uint16_t(result);	\
	zero_op(status.zero_result_, b);	\
	status.extend_flag_ = status.carry_flag_ = uint_fast32_t(result & ~0xffff);	\
	status.negative_flag_ = result & 0x8000;	\
	status.overflow_flag_ = overflow() & 0x8000;

#define addsubl(a, b, op, overflow, x, zero_op)	\
	const uint64_t source = a;	\
	const uint64_t destination = b;	\
	const auto result = op(destination, source, x);	\
	\
	b = uint32_t(result);	\
	zero_op(status.zero_result_, b);	\
	status.extend_flag_ = status.carry_flag_ = uint_fast32_t(result >> 32);	\
	status.negative_flag_ = result & 0x80000000;	\
	status.overflow_flag_ = overflow() & 0x80000000;

#define addb(a, b, x, z) addsubb(a, b, addop, add_overflow, x, z)
#define subb(a, b, x, z) addsubb(a, b, subop, sub_overflow, x, z)
#define addw(a, b, x, z) addsubw(a, b, addop, add_overflow, x, z)
#define subw(a, b, x, z) addsubw(a, b, subop, sub_overflow, x, z)
#define addl(a, b, x, z) addsubl(a, b, addop, add_overflow, x, z)
#define subl(a, b, x, z) addsubl(a, b, subop, sub_overflow, x, z)

#define no_extend(op, a, b)	op(a, b, 0, z_set)
#define extend(op, a, b)	op(a, b, status.extend_flag_, z_or)

		// ADD and ADDA add two quantities, the latter sign extending and without setting any flags;
		// ADDQ and SUBQ act as ADD and SUB, but taking the second argument from the instruction code.
		case Operation::ADDb: {
			no_extend(	addb,
						src.b,
						dest.b);
		} break;

		case Operation::ADDXb: {
			extend(		addb,
						src.b,
						dest.b);
		} break;

		case Operation::ADDw: {
			no_extend(	addw,
						src.w,
						dest.w);
		} break;

		case Operation::ADDXw: {
			extend(		addw,
						src.w,
						dest.w);
		} break;

		case Operation::ADDl: {
			no_extend(	addl,
						src.l,
						dest.l);
		} break;

		case Operation::ADDXl: {
			extend(		addl,
						src.l,
						dest.l);
		} break;

		case Operation::SUBb: {
			no_extend(	subb,
						src.b,
						dest.b);
		} break;

		case Operation::SUBXb: {
			extend(		subb,
						src.b,
						dest.b);
		} break;

		case Operation::SUBw: {
			no_extend(	subw,
						src.w,
						dest.w);
		} break;

		case Operation::SUBXw: {
			extend(		subw,
						src.w,
						dest.w);
		} break;

		case Operation::SUBl: {
			no_extend(	subl,
						src.l,
						dest.l);
		} break;

		case Operation::SUBXl: {
			extend(		subl,
						src.l,
						dest.l);
		} break;

#undef addl
#undef addw
#undef addb
#undef subl
#undef subw
#undef subb
#undef addsubl
#undef addsubw
#undef addsubb
#undef z_set
#undef z_or
#undef no_extend
#undef extend
#undef addop
#undef subop

		case Operation::ADDAw:
			dest.l += u_extend16(src.w);
		break;

		case Operation::ADDAl:
			dest.l += src.l;
		break;

		case Operation::SUBAw:
			dest.l -= u_extend16(src.w);
		break;

		case Operation::SUBAl:
			dest.l -= src.l;
		break;


		// BRA: alters the program counter, exclusively via the prefetch queue.
//		case Operation::BRA: {
//			const int8_t byte_offset = int8_t(prefetch_queue_.halves.high.halves.low);
//
//			// A non-zero offset byte branches by just that amount; otherwise use the word
//			// after as an offset. In both cases, treat as signed.
//			if(byte_offset) {
//				program_counter_.l += uint32_t(byte_offset);
//			} else {
//				program_counter_.l += u_extend16(prefetch_queue_.w);
//			}
//			program_counter_.l -= 2;
//		} break;

		// Two BTSTs: set the zero flag according to the value of the destination masked by
		// the bit named in the source modulo the operation size.
//		case Operation::BTSTb:
//			status.zero_result_ = dest.l & (1 << (src.l & 7));
//		break;
//
//		case Operation::BTSTl:
//			zero_result_ = dest.l & (1 << (src.l & 31));
//		break;
//
//		case Operation::BCLRb:
//			zero_result_ = dest.l & (1 << (src.l & 7));
//			dest.l &= ~(1 << (src.l & 7));
//		break;
//
//		case Operation::BCLRl:
//			zero_result_ = dest.l & (1 << (src.l & 31));
//			dest.l &= ~(1 << (src.l & 31));
//
//			// Clearing in the top word requires an extra four cycles.
//			set_next_microcycle_length(HalfCycles(8 + ((src.l & 31) / 16) * 4));
//		break;
//
//		case Operation::BCHGl:
//			zero_result_ = dest.l & (1 << (src.l & 31));
//			dest.l ^= 1 << (src.l & 31);
//			set_next_microcycle_length(HalfCycles(4 + (((src.l & 31) / 16) * 4)));
//		break;
//
//		case Operation::BCHGb:
//			zero_result_ = dest.b & (1 << (src.l & 7));
//			dest.b ^= 1 << (src.l & 7);
//		break;
//
//		case Operation::BSETl:
//			zero_result_ = dest.l & (1 << (src.l & 31));
//			dest.l |= 1 << (src.l & 31);
//			set_next_microcycle_length(HalfCycles(4 + (((src.l & 31) / 16) * 4)));
//		break;
//
//		case Operation::BSETb:
//			zero_result_ = dest.b & (1 << (src.l & 7));
//			dest.b |= 1 << (src.l & 7);
//		break;

		// Bcc: ordinarily evaluates the relevant condition and displacement size and then:
		//	if condition is false, schedules bus operations to get past this instruction;
		//	otherwise applies the offset and schedules bus operations to refill the prefetch queue.
		//
		// Special case: the condition code is 1, which is ordinarily false. In that case this
		// is the trailing step of a BSR.
//		case Operation::Bcc: {
//			// Grab the 8-bit offset.
//			const int8_t byte_offset = int8_t(prefetch_queue_.halves.high.halves.low);
//
//			// Check whether this is secretly BSR.
//			const bool is_bsr = ((decoded_instruction_.l >> 8) & 0xf) == 1;
//
//			// Test the conditional, treating 'false' as true.
//			const bool should_branch = is_bsr || evaluate_condition(decoded_instruction_.l >> 8);
//
//			// Schedule something appropriate, by rewriting the program for this instruction temporarily.
//			if(should_branch) {
//				if(byte_offset) {
//					program_counter_.l += decltype(program_counter_.l)(byte_offset);
//				} else {
//					program_counter_.l += u_extend16(prefetch_queue_.w);
//				}
//				program_counter_.l -= 2;
//				bus_program = is_bsr ? bsr_bus_steps_ : branch_taken_bus_steps_;
//			} else {
//				if(byte_offset) {
//					bus_program = branch_byte_not_taken_bus_steps_;
//				} else {
//					bus_program = branch_word_not_taken_bus_steps_;
//				}
//			}
//		} break;
//
//		case Operation::DBcc: {
//			// Decide what sort of DBcc this is.
//			if(!evaluate_condition(decoded_instruction_.l >> 8)) {
//				-- src.w;
//				const auto target_program_counter = program_counter_.l + u_extend16(prefetch_queue_.w) - 2;
//
//				if(src.w == 0xffff) {
//					// This DBcc will be ignored as the counter has underflowed.
//					// Schedule n np np np and continue. Assumed: the first np
//					// is from where the branch would have been if taken?
//					bus_program = dbcc_condition_false_no_branch_steps_;
//					dbcc_false_address_ = target_program_counter;
//				} else {
//					// Take the branch. Change PC and schedule n np np.
//					bus_program = dbcc_condition_false_branch_steps_;
//					program_counter_.l = target_program_counter;
//				}
//			} else {
//				// This DBcc will be ignored as the condition is true;
//				// perform nn np np and continue.
//				bus_program = dbcc_condition_true_steps_;
//			}
//		} break;

		case Operation::Scc: {
			dest.b =
				status.evaluate_condition(src.l) ? 0xff : 0x00;
		} break;

		/*
			CLRs: store 0 to the destination, set the zero flag, and clear
			negative, overflow and carry.
		*/
		case Operation::CLRb:
			dest.b = 0;
			status.negative_flag_ = status.overflow_flag_ = status.carry_flag_ = status.zero_result_ = 0;
		break;

		case Operation::CLRw:
			dest.w = 0;
			status.negative_flag_ = status.overflow_flag_ = status.carry_flag_ = status.zero_result_ = 0;
		break;

		case Operation::CLRl:
			dest.l = 0;
			status.negative_flag_ = status.overflow_flag_ = status.carry_flag_ = status.zero_result_ = 0;
		break;

		/*
			CMP.b, CMP.l and CMP.w: sets the condition flags (other than extend) based on a subtraction
			of the source from the destination; the result of the subtraction is not stored.
		*/
		case Operation::CMPb: {
			const uint8_t source = src.b;
			const uint8_t destination = dest.b;
			const int result = destination - source;

			status.zero_result_ = result & 0xff;
			status.carry_flag_ = decltype(status.carry_flag_)(result & ~0xff);
			status.negative_flag_ = result & 0x80;
			status.overflow_flag_ = sub_overflow() & 0x80;
		} break;

		case Operation::CMPw: {
			const uint16_t source = src.w;
			const uint16_t destination = src.w;
			const int result = destination - source;

			status.zero_result_ = result & 0xffff;
			status.carry_flag_ = decltype(status.carry_flag_)(result & ~0xffff);
			status.negative_flag_ = result & 0x8000;
			status.overflow_flag_ = sub_overflow() & 0x8000;
		} break;

		case Operation::CMPAw: {
			const auto source = uint64_t(u_extend16(src.w));
			const uint64_t destination = dest.l;
			const auto result = destination - source;

			status.zero_result_ = uint32_t(result);
			status.carry_flag_ = result >> 32;
			status.negative_flag_ = result & 0x80000000;
			status.overflow_flag_ = sub_overflow() & 0x80000000;
		} break;

		case Operation::CMPl: {
			const auto source = uint64_t(src.l);
			const auto destination = uint64_t(dest.l);
			const auto result = destination - source;

			status.zero_result_ = uint32_t(result);
			status.carry_flag_ = result >> 32;
			status.negative_flag_ = result & 0x80000000;
			status.overflow_flag_ = sub_overflow() & 0x80000000;
		} break;

		// JMP: copies EA(0) to the program counter.
//		case Operation::JMP:
//			program_counter_ = effective_address_[0];
//		break;

		// JMP: copies the source bus data to the program counter.
//		case Operation::RTS:
//			program_counter_ = source_bus_data_;
//		break;

		/*
			MOVE.b, MOVE.l and MOVE.w: move the least significant byte or word, or the entire long word,
			and set negative, zero, overflow and carry as appropriate.
		*/
		case Operation::MOVEb:
			status.zero_result_ = dest.b = src.b;
			status.negative_flag_ = status.zero_result_ & 0x80;
			status.overflow_flag_ = status.carry_flag_ = 0;
		break;

		case Operation::MOVEw:
			status.zero_result_ = dest.w = src.w;
			status.negative_flag_ = status.zero_result_ & 0x8000;
			status.overflow_flag_ = status.carry_flag_ = 0;
		break;

		case Operation::MOVEl:
			status.zero_result_ = dest.l = src.l;
			status.negative_flag_ = status.zero_result_ & 0x80000000;
			status.overflow_flag_ = status.carry_flag_ = 0;
		break;

		/*
			MOVEA.l: move the entire long word;
			MOVEA.w: move the least significant word and sign extend it.
			Neither sets any flags.
		*/
		case Operation::MOVEAw:
			dest.l = u_extend16(src.w);
		break;

		case Operation::MOVEAl:
			dest.l = src.l;
		break;

//		case Operation::PEA:
//			destination_bus_data_ = effective_address_[0];
//		break;

		/*
			Status word moves and manipulations.
		*/

		case Operation::MOVEtoSR:
			status.set_status(src.w);
		break;

		case Operation::MOVEfromSR:
			dest.w = status.status();
		break;

		case Operation::MOVEtoCCR:
			status.set_ccr(src.w);
		break;

		case Operation::EXTbtow:
			dest.w = uint16_t(int8_t(dest.b));
			status.overflow_flag_ = status.carry_flag_ = 0;
			status.zero_result_ = dest.w;
			status.negative_flag_ = status.zero_result_ & 0x8000;
		break;

		case Operation::EXTwtol:
			dest.l = u_extend16(dest.w);
			status.overflow_flag_ = status.carry_flag_ = 0;
			status.zero_result_ = dest.l;
			status.negative_flag_ = status.zero_result_ & 0x80000000;
		break;

#define and_op(a, b) a &= b
#define or_op(a, b) a |= b
#define eor_op(a, b) a ^= b

#define apply(op, func)	{			\
	auto sr = status.status();		\
	op(sr, src.w);	\
	status.func(sr);				\
}

#define apply_op_sr(op)		apply(op, set_status)
#define apply_op_ccr(op)	apply(op, set_ccr)

		case Operation::ANDItoSR:	apply_op_sr(and_op);	break;
		case Operation::EORItoSR:	apply_op_sr(eor_op);	break;
		case Operation::ORItoSR:	apply_op_sr(or_op);		break;

		case Operation::ANDItoCCR:	apply_op_ccr(and_op);	break;
		case Operation::EORItoCCR:	apply_op_ccr(eor_op);	break;
		case Operation::ORItoCCR:	apply_op_ccr(or_op);	break;

#undef apply_op_ccr
#undef apply_op_sr
#undef apply
#undef eor_op
#undef or_op
#undef and_op

		/*
			Multiplications.
		*/

		case Operation::MULU: {
			dest.l = dest.w * src.w;
			status.carry_flag_ = status.overflow_flag_ = 0;
			status.zero_result_ = dest.l;
			status.negative_flag_ = status.zero_result_ & 0x80000000;

			int number_of_ones = src.w;
			convert_to_bit_count_16(number_of_ones);

			// Time taken = 38 cycles + 2 cycles for every 1 in the source.
			flow_controller.consume_cycles(2 * number_of_ones + 34);
		} break;

		case Operation::MULS: {
			dest.l =
				u_extend16(dest.w) * u_extend16(src.w);
			status.carry_flag_ = status.overflow_flag_ = 0;
			status.zero_result_ = dest.l;
			status.negative_flag_ = status.zero_result_ & 0x80000000;

			// Find the number of 01 or 10 pairs in the 17-bit number
			// formed by the source value with a 0 suffix.
			int number_of_pairs = src.w;
			number_of_pairs = (number_of_pairs ^ (number_of_pairs << 1)) & 0xffff;
			convert_to_bit_count_16(number_of_pairs);

			// Time taken = 38 cycles + 2 cycles per 1 in the source.
			flow_controller.consume_cycles(2 * number_of_pairs + 34);
		} break;

		/*
			Divisions.
		*/

#define announce_divide_by_zero()									\
	status.negative_flag_ = status.overflow_flag_ = 0;				\
	status.zero_result_ = 1;										\
	flow_controller.raise_exception(5);

		case Operation::DIVU: {
			status.carry_flag_ = 0;

			// An attempt to divide by zero schedules an exception.
			if(!src.w) {
				// Schedule a divide-by-zero exception.
				announce_divide_by_zero();
				return;
			}

			uint32_t dividend = dest.l;
			uint32_t divisor = src.w;
			const auto quotient = dividend / divisor;

			// If overflow would occur, appropriate flags are set and the result is not written back.
			if(quotient > 65535) {
				status.overflow_flag_ = status.zero_result_ = status.negative_flag_ = 1;
				flow_controller.consume_cycles(3*2);
				return;
			}

			const uint16_t remainder = uint16_t(dividend % divisor);
			dest.l = uint32_t((remainder << 16) | uint16_t(quotient));

			status.overflow_flag_ = 0;
			status.zero_result_ = quotient;
			status.negative_flag_ = status.zero_result_ & 0x8000;

			// Calculate cost; this is based on the flowchart in yacht.txt.
			// I could actually calculate the division result here, since this is
			// a classic divide algorithm, but would rather that errors produce
			// incorrect timing only, not incorrect timing plus incorrect results.
			int cycles_expended = 12;	// Covers the nn n to get into the loop.

			divisor <<= 16;
			for(int c = 0; c < 15; ++c) {
				if(dividend & 0x80000000) {
					dividend = (dividend << 1) - divisor;
					cycles_expended += 4;	// Easy; just the fixed nn iteration cost.
				} else {
					dividend <<= 1;

					// Yacht.txt, and indeed a real microprogram, would just subtract here
					// and test the sign of the result, but this is easier to follow:
					if (dividend >= divisor) {
						dividend -= divisor;
						cycles_expended += 6;	// i.e. the original nn plus one further n before going down the MSB=0 route.
					} else {
						cycles_expended += 8;	// The costliest path (since in real life it's a subtraction and then a step
												// back from there) — all costs accrue. So the fixed nn loop plus another n,
												// plus another one.
					}
				}
			}
			flow_controller.consume_cycles(cycles_expended);
		} break;

		case Operation::DIVS: {
			status.carry_flag_ = 0;

			// An attempt to divide by zero schedules an exception.
			if(!src.w) {
				// Schedule a divide-by-zero exception.
				announce_divide_by_zero()
				break;
			}

			const int32_t signed_dividend = int32_t(dest.l);
			const int32_t signed_divisor = s_extend16(src.w);
			const auto result_sign =
				( (0 <= signed_dividend) - (signed_dividend < 0) ) *
				( (0 <= signed_divisor) - (signed_divisor < 0) );

			const uint32_t dividend = uint32_t(std::abs(signed_dividend));
			const uint32_t divisor = uint32_t(std::abs(signed_divisor));

			int cycles_expended = 12;	// Covers the nn nnn n to get beyond the sign test.
			if(signed_dividend < 0) {
				cycles_expended += 2;	// An additional microycle applies if the dividend is negative.
			}

			// Check for overflow. If it exists, work here is already done.
			const auto quotient = dividend / divisor;
			if(quotient > 32767) {
				status.overflow_flag_ = 1;
				flow_controller.consume_cycles(6*2);
				break;
			}

			const uint16_t remainder = uint16_t(signed_dividend % signed_divisor);
			const int signed_quotient = result_sign*int(quotient);
			dest.l = uint32_t((remainder << 16) | uint16_t(signed_quotient));

			status.zero_result_ = decltype(status.zero_result_)(signed_quotient);
			status.negative_flag_ = status.zero_result_ & 0x8000;
			status.overflow_flag_ = 0;

			// Algorithm here: there is a fixed cost per unset bit
			// in the first 15 bits of the unsigned quotient.
			auto positive_quotient_bits = ~quotient & 0xfffe;
			convert_to_bit_count_16(positive_quotient_bits);
			cycles_expended += 2 * positive_quotient_bits;

			// There's then no way to terminate the loop that isn't at least ten cycles long;
			// there's also a fixed overhead per bit. The two together add up to the 104 below.
			cycles_expended += 104;

			// This picks up at 'No more bits' in yacht.txt's diagram.
			if(signed_divisor < 0) {
				cycles_expended += 2;
			} else if(signed_dividend < 0) {
				cycles_expended += 4;
			}
			flow_controller.consume_cycles(cycles_expended);
		} break;

#undef announce_divide_by_zero

		/*
			MOVEP: move words and long-words a byte at a time.
		*/

//		case Operation::MOVEPtoMw:
//			// Write pattern is nW+ nw, which should write the low word of the source in big-endian form.
//			destination_bus_data_.halves.high.l = src.halves.low.halves.high;
//			destination_bus_data_.w = src.b;
//		break;
//
//		case Operation::MOVEPtoMl:
//			// Write pattern is nW+ nWr+ nw+ nwr, which should write the source in big-endian form.
//			destination_bus_data_.halves.high.l = src.halves.high.halves.high;
//			source_bus_data_.halves.high.l = src.halves.high.halves.low;
//			destination_bus_data_.w = src.halves.low.halves.high;
//			source_bus_data_.w = src.b;
//		break;
//
//		case Operation::MOVEPtoRw:
//			// Read pattern is nRd+ nrd.
//			src.halves.low.halves.high = destination_bus_data_.halves.high.halves.low;
//			src.b = destination_bus_data_.b;
//		break;
//
//		case Operation::MOVEPtoRl:
//			// Read pattern is nRd+ nR+ nrd+ nr.
//			src.halves.high.halves.high = destination_bus_data_.halves.high.halves.low;
//			src.halves.high.halves.low = source_bus_data_.halves.high.halves.low;
//			src.halves.low.halves.high = destination_bus_data_.b;
//			src.b = source_bus_data_.b;
//		break;

		/*
			MOVEM: multi-word moves.
		*/

//#define setup_movem(words_per_reg, base)								\
//	/* Count the number of long words to move.	 */						\
//	size_t total_to_move = 0;											\
//	auto mask = next_word_;												\
//	while(mask) {														\
//		total_to_move += mask&1;										\
//		mask >>= 1;														\
//	}																	\
//																		\
//	/* Twice that many words plus one will need to be moved */			\
//	bus_program = base + (64 - total_to_move*words_per_reg)*2;			\
//																		\
//	/* Fill in the proper addresses and targets. */						\
//	const auto mode = (decoded_instruction_.l >> 3) & 7;				\
//	uint32_t start_address;												\
//	if(mode <= 4) {														\
//		start_address = destination_address().l;						\
//	} else {															\
//		start_address = effective_address_[1].l;						\
//	}																	\
//																		\
//auto step = bus_program;											\
//uint32_t *address_storage = precomputed_addresses_;					\
//mask = next_word_;													\
//int offset = 0;
//
//#define inc_action(x, v) x += v
//#define dec_action(x, v) x -= v
//
//#define write_address_sequence_long(action, l)														\
//while(mask) {																					\
//if(mask&1) {																				\
//address_storage[0] = start_address;														\
//action(start_address, 2);																\
//address_storage[1] = start_address;														\
//action(start_address, 2);																\
//																			\
//step[0].microcycle.address = step[1].microcycle.address = address_storage;				\
//step[2].microcycle.address = step[3].microcycle.address = address_storage + 1;			\
//																			\
//const auto target = (offset > 7) ? &address_[offset&7] : &data_[offset];				\
//step[l].microcycle.value = step[l+1].microcycle.value = &target->halves.high;			\
//step[(l^2)].microcycle.value = step[(l^2)+1].microcycle.value = &target->halves.low;	\
//																			\
//address_storage += 2;																	\
//step += 4;																				\
//}																							\
//mask >>= 1;																					\
//action(offset, 1);																			\
//}
//
//#define write_address_sequence_word(action)												\
//while(mask) {																		\
//if(mask&1) {																	\
//address_storage[0] = start_address;											\
//action(start_address, 2);													\
//																\
//step[0].microcycle.address = step[1].microcycle.address = address_storage;	\
//																\
//const auto target = (offset > 7) ? &address_[offset&7] : &data_[offset];	\
//step[0].microcycle.value = step[1].microcycle.value = &target->halves.low;	\
//																\
//++ address_storage;															\
//step += 2;																	\
//}																				\
//mask >>= 1;																		\
//action(offset, 1);																\
//}
//
//		case Operation::MOVEMtoRl: {
//			setup_movem(2, movem_read_steps_);
//
//			// Everything for move to registers is based on an incrementing
//			// address; per M68000PRM:
//			//
//			// "[If using the postincrement addressing mode then] the incremented address
//			// register contains the address of the last operand loaded plus the operand length.
//			// If the addressing register is also loaded from memory, the memory value is ignored
//			// and the register is written with the postincremented effective address."
//			//
//			// The latter part is dealt with by MicroOp::Action::MOVEMtoRComplete, which also
//			// does any necessary sign extension.
//			write_address_sequence_long(inc_action, 0);
//
//			// MOVEM to R always reads one word too many.
//			address_storage[0] = start_address;
//			step[0].microcycle.address = step[1].microcycle.address = address_storage;
//			step[0].microcycle.value = step[1].microcycle.value = &throwaway_value_;
//			movem_final_address_ = start_address;
//		} break;
//
//		case Operation::MOVEMtoRw: {
//			setup_movem(1, movem_read_steps_);
//			write_address_sequence_word(inc_action);
//
//			// MOVEM to R always reads one word too many.
//			address_storage[0] = start_address;
//			step[0].microcycle.address = step[1].microcycle.address = address_storage;
//			step[0].microcycle.value = step[1].microcycle.value = &throwaway_value_;
//			movem_final_address_ = start_address;
//		} break;
//
//		case Operation::MOVEMtoMl: {
//			setup_movem(2, movem_write_steps_);
//
//			// MOVEM to M counts downwards and enumerates the registers in reverse order
//			// if subject to the predecrementing mode; otherwise it counts upwards and
//			// operates exactly as does MOVEM to R.
//			//
//			// Note also: "The MC68000 and MC68010 write the initial register value
//			// (not decremented) [when writing a register that is providing
//			// pre-decrementing addressing]."
//			//
//			// Hence the decrementing register (if any) is updated
//			// by MicroOp::Action::MOVEMtoMComplete.
//			if(mode == 4) {
//				offset = 15;
//				start_address -= 2;
//				write_address_sequence_long(dec_action, 2);
//				movem_final_address_ = start_address + 2;
//			} else {
//				write_address_sequence_long(inc_action, 0);
//			}
//		} break;
//
//		case Operation::MOVEMtoMw: {
//			setup_movem(1, movem_write_steps_);
//
//			if(mode == 4) {
//				offset = 15;
//				start_address -= 2;
//				write_address_sequence_word(dec_action);
//				movem_final_address_ = start_address + 2;
//			} else {
//				write_address_sequence_word(inc_action);
//			}
//		} break;
//
//#undef setup_movem
//#undef write_address_sequence_long
//#undef write_address_sequence_word
//#undef inc_action
//#undef dec_action

		// TRAP, which is a nicer form of ILLEGAL.
//		case Operation::TRAP: {
			// Select the trap steps as next; the initial microcycle should be 4 cycles long.
//			bus_program = trap_steps_;
//			populate_trap_steps((decoded_instruction_.l & 15) + 32, status());
//			set_next_microcycle_length(HalfCycles(12));

			// The program counter to push is actually one slot ago.
//			program_counter_.l -= 2;
//		} break;

//		case Operation::TRAPV: {
//			if(overflow_flag_) {
//				// Select the trap steps as next; the initial microcycle should be skipped.
//				bus_program = trap_steps_;
//				populate_trap_steps(7, status());
//				set_next_microcycle_length(HalfCycles(4));
//
//				// Push the address after the TRAPV.
//				program_counter_.l -= 4;
//			}
//		} break;

		case Operation::CHK: {
			const bool is_under = s_extend16(dest.w) < 0;
			const bool is_over = s_extend16(dest.w) > s_extend16(src.w);

			status.overflow_flag_ = status.carry_flag_ = 0;
			status.zero_result_ = dest.w;

			// Test applied for N:
			//
			//	if Dn < 0, set negative flag;
			//	otherwise, if Dn > <ea>, reset negative flag.
			if(is_over) status.negative_flag_ = 0;
			if(is_under) status.negative_flag_ = 1;

			// No exception is the default course of action; deviate only if an
			// exception is necessary.
			if(is_under || is_over) {
//				bus_program = trap_steps_;
//				populate_trap_steps(6, status());
//				if(is_over) {
//					set_next_microcycle_length(HalfCycles(20));
//				} else {
//					set_next_microcycle_length(HalfCycles(24));
//				}

				// The program counter to push is two slots ago as whatever was the correct prefetch
				// to continue without an exception has already happened, just in case.
//				program_counter_.l -= 4;
				assert(false);
				// TODO.
			}
		} break;

		/*
			NEGs: negatives the destination, setting the zero,
			negative, overflow and carry flags appropriate, and extend.

			NB: since the same logic as SUB is used to calculate overflow,
			and SUB calculates `destination - source`, the NEGs deliberately
			label 'source' and 'destination' differently from Motorola.
		*/
		case Operation::NEGb: {
			const int destination = 0;
			const int source = dest.b;
			const auto result = destination - source;
			dest.b = uint8_t(result);

			status.zero_result_ = result & 0xff;
			status.extend_flag_ = status.carry_flag_ = decltype(status.carry_flag_)(result & ~0xff);
			status.negative_flag_ = result & 0x80;
			status.overflow_flag_ = sub_overflow() & 0x80;
		} break;

		case Operation::NEGw: {
			const int destination = 0;
			const int source = dest.w;
			const auto result = destination - source;
			dest.w = uint16_t(result);

			status.zero_result_ = result & 0xffff;
			status.extend_flag_ = status.carry_flag_ = decltype(status.carry_flag_)(result & ~0xffff);
			status.negative_flag_ = result & 0x8000;
			status.overflow_flag_ = sub_overflow() & 0x8000;
		} break;

		case Operation::NEGl: {
			const uint64_t destination = 0;
			const uint64_t source = dest.l;
			const auto result = destination - source;
			dest.l = uint32_t(result);

			status.zero_result_ = uint_fast32_t(result);
			status.extend_flag_ = status.carry_flag_ = result >> 32;
			status.negative_flag_ = result & 0x80000000;
			status.overflow_flag_ = sub_overflow() & 0x80000000;
		} break;

		/*
			NEGXs: NEG, with extend.
		*/
		case Operation::NEGXb: {
			const int source = dest.b;
			const int destination = 0;
			const auto result = destination - source - (status.extend_flag_ ? 1 : 0);
			dest.b = uint8_t(result);

			status.zero_result_ |= result & 0xff;
			status.extend_flag_ = status.carry_flag_ = decltype(status.carry_flag_)(result & ~0xff);
			status.negative_flag_ = result & 0x80;
			status.overflow_flag_ = sub_overflow() & 0x80;
		} break;

		case Operation::NEGXw: {
			const int source = dest.w;
			const int destination = 0;
			const auto result = destination - source - (status.extend_flag_ ? 1 : 0);
			dest.w = uint16_t(result);

			status.zero_result_ |= result & 0xffff;
			status.extend_flag_ = status.carry_flag_ = decltype(status.carry_flag_)(result & ~0xffff);
			status.negative_flag_ = result & 0x8000;
			status.overflow_flag_ = sub_overflow() & 0x8000;
		} break;

		case Operation::NEGXl: {
			const uint64_t source = dest.l;
			const uint64_t destination = 0;
			const auto result = destination - source - (status.extend_flag_ ? 1 : 0);
			dest.l = uint32_t(result);

			status.zero_result_ |= uint_fast32_t(result);
			status.extend_flag_ = status.carry_flag_ = result >> 32;
			status.negative_flag_ = result & 0x80000000;
			status.overflow_flag_ = sub_overflow() & 0x80000000;
		} break;

		/*
			The no-op.
		*/
		case Operation::NOP:
		break;

		/*
			LINK and UNLINK help with stack frames, allowing a certain
			amount of stack space to be allocated or deallocated.
		*/

//		case Operation::LINK:
//			// Make space for the new long-word value, and set up
//			// the proper target address for the stack operations to follow.
//			address_[7].l -= 4;
//			effective_address_[1].l = address_[7].l;
//
//			// The current value of the address register will be pushed.
//			destination_bus_data_.l = src.l;
//
//			// The address register will then contain the bottom of the stack,
//			// and the stack pointer will be offset.
//			src.l = address_[7].l;
//			address_[7].l += u_extend16(prefetch_queue_.w);
//		break;
//
//		case Operation::UNLINK:
//			address_[7].l = effective_address_[1].l + 2;
//			dest.l = destination_bus_data_.l;
//		break;

		/*
			TAS: sets zero and negative depending on the current value of the destination,
			and sets the high bit.
		*/

		case Operation::TAS:
			status.overflow_flag_ = status.carry_flag_ = 0;
			status.zero_result_ = dest.b;
			status.negative_flag_ = dest.b & 0x80;
			dest.b |= 0x80;
		break;

		/*
			Bitwise operators: AND, OR and EOR. All three clear the overflow and carry flags,
			and set zero and negative appropriately.
		*/
#define op_and(x, y)	x &= y
#define op_or(x, y)		x |= y
#define op_eor(x, y)	x ^= y

#define bitwise(source, dest, sign_mask, operator)	\
	operator(dest, source);							\
	status.overflow_flag_ = status.carry_flag_ = 0;	\
	status.zero_result_ = dest;						\
	status.negative_flag_ = dest & sign_mask;

#define andx(source, dest, sign_mask)	bitwise(source, dest, sign_mask, op_and)
#define eorx(source, dest, sign_mask)	bitwise(source, dest, sign_mask, op_eor)
#define orx(source, dest, sign_mask)	bitwise(source, dest, sign_mask, op_or)

#define op_bwl(name, op)											\
	case Operation::name##b: op(src.b, dest.b, 0x80);		break;	\
	case Operation::name##w: op(src.w, dest.w, 0x8000);		break;	\
	case Operation::name##l: op(src.l, dest.l, 0x80000000);	break;

		op_bwl(AND, andx);
		op_bwl(EOR, eorx);
		op_bwl(OR, orx);

#undef op_bwl
#undef orx
#undef eorx
#undef andx
#undef bitwise
#undef op_eor
#undef op_or
#undef op_and

		// NOTs: take the logical inverse, affecting the negative and zero flags.
		case Operation::NOTb:
			dest.b ^= 0xff;
			status.zero_result_ = dest.b;
			status.negative_flag_ = status.zero_result_ & 0x80;
			status.overflow_flag_ = status.carry_flag_ = 0;
		break;

		case Operation::NOTw:
			dest.w ^= 0xffff;
			status.zero_result_ = dest.w;
			status.negative_flag_ = status.zero_result_ & 0x8000;
			status.overflow_flag_ = status.carry_flag_ = 0;
		break;

		case Operation::NOTl:
			dest.l ^= 0xffffffff;
			status.zero_result_ = dest.l;
			status.negative_flag_ = status.zero_result_ & 0x80000000;
			status.overflow_flag_ = status.carry_flag_ = 0;
		break;

#define sbcd()																							\
	/* Perform the BCD arithmetic by evaluating the two nibbles separately. */							\
	const int unadjusted_result = destination - source - (status.extend_flag_ ? 1 : 0);					\
	int result = (destination & 0xf) - (source & 0xf) - (status.extend_flag_ ? 1 : 0);					\
	if((result & 0x1f) > 0x09) result -= 0x06;															\
	result += (destination & 0xf0) - (source & 0xf0);													\
	status.extend_flag_ = status.carry_flag_ = decltype(status.carry_flag_)((result & 0x1ff) > 0x99);	\
	if(status.carry_flag_) result -= 0x60;																\
																										\
	/* Set all flags essentially as if this were normal subtraction. */									\
	status.zero_result_ |= result & 0xff;																\
	status.negative_flag_ = result & 0x80;																\
	status.overflow_flag_ = unadjusted_result & ~result & 0x80;											\
																										\
	/* Store the result. */																				\
	dest.b = uint8_t(result);

		/*
			SBCD subtracts the lowest byte of the source from that of the destination using
			BCD arithmetic, obeying the extend flag.
		*/
		case Operation::SBCD: {
			const uint8_t source = src.b;
			const uint8_t destination = dest.b;
			sbcd();
		} break;

		/*
			NBCD is like SBCD except that the result is 0 - destination rather than
			destination - source.
		*/
		case Operation::NBCD: {
			const uint8_t source = dest.b;
			const uint8_t destination = 0;
			sbcd();
		} break;

		// EXG and SWAP exchange/swap words or long words.

		case Operation::EXG: {
			const auto temporary = src.l;
			src.l = dest.l;
			dest.l = temporary;
		} break;

		case Operation::SWAP: {
			uint16_t *const words = reinterpret_cast<uint16_t *>(&dest);
			const auto temporary = words[0];
			words[0] = words[1];
			words[1] = temporary;

			status.zero_result_ = dest.l;
			status.negative_flag_ = temporary & 0x8000;
			status.overflow_flag_ = status.carry_flag_ = 0;
		} break;

		/*
			Shifts and rotates.
		*/
#define set_neg_zero(v, m)											\
	status.zero_result_ = decltype(status.zero_result_)(v);						\
	status.negative_flag_ = status.zero_result_ & decltype(status.negative_flag_)(m);

#define set_neg_zero_overflow(v, m)									\
	set_neg_zero(v, m);												\
	status.overflow_flag_ = (decltype(status.zero_result_)(value) ^ status.zero_result_) & decltype(status.overflow_flag_)(m);

#define decode_shift_count()	\
	int shift_count = (decoded_instruction_.l & 32) ? data_[(decoded_instruction_.l >> 9) & 7].l&63 : ( ((decoded_instruction_.l >> 9)&7) ? ((decoded_instruction_.l >> 9)&7) : 8) ;	\
	flow_controller.consume_cycles(2 * shift_count);

#define set_flags_b(t) set_flags(dest.b, 0x80, t)
#define set_flags_w(t) set_flags(dest.w, 0x8000, t)
#define set_flags_l(t) set_flags(dest.l, 0x80000000, t)

#define asl(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag_ = status.overflow_flag_ = 0;	\
	} else {	\
		destination = (shift_count < size) ? decltype(destination)(value << shift_count) : 0;	\
		status.status.extend_flag_ = status.carry_flag_ = decltype(status.carry_flag_)(value) & decltype(status.carry_flag_)( (1u << (size - 1)) >> (shift_count - 1) );	\
		\
		if(shift_count >= size) status.overflow_flag_ = value && (value != decltype(value)(-1));	\
		else {	\
			const auto mask = decltype(destination)(0xffffffff << (size - shift_count));	\
			status.overflow_flag_ = mask & value && ((mask & value) != mask);	\
		}	\
	}	\
	\
	set_neg_zero(destination, 1 << (size - 1));	\
}

		case Operation::ASLm: {
			const auto value = dest.w;
			dest.w = uint16_t(value << 1);
			status.extend_flag_ = status.carry_flag_ = value & 0x8000;
			set_neg_zero_overflow(dest.w, 0x8000);
		} break;
//		case Operation::ASLb: asl(dest.b, 8);	break;
//		case Operation::ASLw: asl(dest.w, 16); 		break;
//		case Operation::ASLl: asl(dest.l, 32); 					break;

#define asr(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		carry_flag_ = 0;	\
	} else {	\
		destination = (shift_count < size) ?	\
		decltype(destination)(\
			(value >> shift_count) |	\
			((value & decltype(value)(1 << (size - 1)) ? 0xffffffff : 0x000000000) << (size - shift_count))	\
		) :	\
			decltype(destination)(	\
			(value & decltype(value)(1 << (size - 1))) ? 0xffffffff : 0x000000000	\
		);	\
		status.extend_flag_ = status.carry_flag_ = decltype(carry_flag_)(value) & decltype(carry_flag_)(1 << (shift_count - 1));	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ASRm: {
			const auto value = dest.w;
			dest.w = (value&0x8000) | (value >> 1);
			status.extend_flag_ = status.carry_flag_ = value & 1;
			set_neg_zero_overflow(dest.w, 0x8000);
		} break;
//		case Operation::ASRb: asr(dest.b, 8);	break;
//		case Operation::ASRw: asr(dest.w, 16); 		break;
//		case Operation::ASRl: asr(dest.l, 32); 					break;


#undef set_neg_zero_overflow
#define set_neg_zero_overflow(v, m)	\
	set_neg_zero(v, m);	\
	status.overflow_flag_ = 0;

#undef set_flags
#define set_flags(v, m, t)	\
	status.zero_result_ = v;	\
	status.negative_flag_ = status.zero_result_ & (m);	\
	status.overflow_flag_ = 0;	\
	status.carry_flag_ = value & (t);

#define lsl(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		carry_flag_ = 0;	\
	} else {	\
		destination = (shift_count < size) ? decltype(destination)(value << shift_count) : 0;	\
		status.extend_flag_ = status.carry_flag_ = decltype(status.carry_flag_)(value) & decltype(status.carry_flag_)( (1u << (size - 1)) >> (shift_count - 1) );	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::LSLm: {
			const auto value = dest.w;
			dest.w = uint16_t(value << 1);
			status.extend_flag_ = status.carry_flag_ = value & 0x8000;
			set_neg_zero_overflow(dest.w, 0x8000);
		} break;
//		case Operation::LSLb: lsl(dest.b, 8);	break;
//		case Operation::LSLw: lsl(dest.w, 16); 		break;
//		case Operation::LSLl: lsl(dest.l, 32); 					break;

#define lsr(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag_ = 0;	\
	} else {	\
		destination = (shift_count < size) ? (value >> shift_count) : 0;	\
		status.extend_flag_ = status.carry_flag_ = value & decltype(status.carry_flag_)(1 << (shift_count - 1));	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::LSRm: {
			const auto value = dest.w;
			dest.w = value >> 1;
			status.extend_flag_ = status.carry_flag_ = value & 1;
			set_neg_zero_overflow(dest.w, 0x8000);
		} break;
//		case Operation::LSRb: lsr(dest.b, 8);	break;
//		case Operation::LSRw: lsr(dest.w, 16); 		break;
//		case Operation::LSRl: lsr(dest.l, 32); 					break;

#define rol(destination, size)	{ \
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag_ = 0;	\
	} else {	\
		shift_count &= (size - 1);	\
		destination = decltype(destination)(	\
			(value << shift_count) |	\
			(value >> (size - shift_count))	\
		);	\
		status.carry_flag_ = decltype(status.carry_flag_)(destination & 1);	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ROLm: {
			const auto value = dest.w;
			dest.w = uint16_t((value << 1) | (value >> 15));
			status.carry_flag_ = dest.w & 1;
			set_neg_zero_overflow(dest.w, 0x8000);
		} break;
//		case Operation::ROLb: rol(dest.b, 8);	break;
//		case Operation::ROLw: rol(dest.w, 16); 		break;
//		case Operation::ROLl: rol(dest.l, 32); 					break;

#define ror(destination, size)	{ \
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag_ = 0;	\
	} else {	\
		shift_count &= (size - 1);	\
		destination = decltype(destination)(\
			(value >> shift_count) |	\
			(value << (size - shift_count))	\
		);\
		status.carry_flag_ = destination & decltype(status.carry_flag_)(1 << (size - 1));	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::RORm: {
			const auto value = dest.w;
			dest.w = uint16_t((value >> 1) | (value << 15));
			status.carry_flag_ = dest.w & 0x8000;
			set_neg_zero_overflow(dest.w, 0x8000);
		} break;
//		case Operation::RORb: ror(dest.b, 8);	break;
//		case Operation::RORw: ror(dest.w, 16); 		break;
//		case Operation::RORl: ror(dest.l, 32); 					break;

#define roxl(destination, size)	{ \
	decode_shift_count();	\
	\
	shift_count %= (size + 1);	\
	uint64_t compound = uint64_t(destination) | (status.extend_flag_ ? (1ull << size) : 0);	\
	compound = \
		(compound << shift_count) |	\
		(compound >> (size + 1 - shift_count));	\
	status.carry_flag_ = status.extend_flag_ = decltype(status.carry_flag_)((compound >> size) & 1);	\
	destination = decltype(destination)(compound);	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ROXLm: {
			const auto value = dest.w;
			dest.w = uint16_t((value << 1) | (status.extend_flag_ ? 0x0001 : 0x0000));
			status.extend_flag_ = value & 0x8000;
			set_flags_w(0x8000);
		} break;
//		case Operation::ROXLb: roxl(dest.b, 8);	break;
//		case Operation::ROXLw: roxl(dest.w, 16); 		break;
//		case Operation::ROXLl: roxl(dest.l, 32); 					break;

#define roxr(destination, size)	{ \
	decode_shift_count();	\
	\
	shift_count %= (size + 1);	\
	uint64_t compound = uint64_t(destination) | (status.extend_flag_ ? (1ull << size) : 0);	\
	compound = \
		(compound >> shift_count) |	\
		(compound << (size + 1 - shift_count));	\
		status.carry_flag_ = status.extend_flag_ = decltype(status.carry_flag_)((compound >> size) & 1);	\
	destination = decltype(destination)(compound);	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ROXRm: {
			const auto value = dest.w;
			dest.w = (value >> 1) | (status.extend_flag_ ? 0x8000 : 0x0000);
			status.extend_flag_ = value & 0x0001;
			set_flags_w(0x0001);
		} break;
//		case Operation::ROXRb: roxr(dest.b, 8);	break;
//		case Operation::ROXRw: roxr(dest.w, 16); 		break;
//		case Operation::ROXRl: roxr(dest.l, 32); 					break;

#undef roxr
#undef roxl
#undef ror
#undef rol
#undef asr
#undef lsr
#undef lsl
#undef asl

#undef set_flags
#undef decode_shift_count
#undef set_flags_b
#undef set_flags_w
#undef set_flags_l
#undef set_neg_zero_overflow
#undef set_net_zero

		/*
			RTE and RTR share an implementation.
		*/
//		case Operation::RTE_RTR:
//			// If this is RTR, patch out the supervisor half of the status register.
//			if(decoded_instruction_.l == 0x4e77) {
//				const auto current_status = status();
//				source_bus_data_.halves.low.halves.high =
//					uint8_t(current_status >> 8);
//			}
//			apply_status(source_bus_data_.l);
//		break;

		/*
			TSTs: compare to zero.
		*/

		case Operation::TSTb:
			status.carry_flag_ = status.overflow_flag_ = 0;
			status.zero_result_ = src.b;
			status.negative_flag_ = status.zero_result_ & 0x80;
		break;

		case Operation::TSTw:
			status.carry_flag_ = status.overflow_flag_ = 0;
			status.zero_result_ = src.w;
			status.negative_flag_ = status.zero_result_ & 0x8000;
		break;

		case Operation::TSTl:
			status.carry_flag_ = status.overflow_flag_ = 0;
			status.zero_result_ = src.l;
			status.negative_flag_ = status.zero_result_ & 0x80000000;
		break;

//		case Operation::STOP:
//			apply_status(prefetch_queue_.w);
//			execution_state_ = ExecutionState::Stopped;
//		break;

		/*
			Development period debugging.
		*/
		default:
			assert(false);
		break;
	}
#undef sub_overflow
#undef add_overflow

#undef u_extend16
#undef u_extend8
#undef s_extend16
#undef s_extend8
#undef convert_to_bit_count_16

}

}
}

#endif /* InstructionSets_M68k_PerformImplementation_h */
