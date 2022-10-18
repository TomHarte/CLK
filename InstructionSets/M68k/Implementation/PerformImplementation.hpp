//
//  PerformImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_PerformImplementation_h
#define InstructionSets_M68k_PerformImplementation_h

#include "../ExceptionVectors.hpp"

#include <cassert>
#include <cmath>

namespace InstructionSet {
namespace M68k {

/// Sign-extend @c x to 32 bits and return as an unsigned 32-bit int.
inline uint32_t u_extend16(uint16_t x)	{	return uint32_t(int16_t(x));	}

/// Sign-extend @c x to 32 bits and return as a signed 32-bit int.
inline int32_t s_extend16(uint16_t x)	{	return int32_t(int16_t(x));		}

namespace Primitive {

/// @returns An int of type @c IntT with only the most-significant bit set.
template <typename IntT> constexpr IntT top_bit() {
	static_assert(!std::numeric_limits<IntT>::is_signed);
	constexpr IntT max = std::numeric_limits<IntT>::max();
	return max - (max >> 1);
}

/// @returns An int with the top bit indicating whether overflow occurred when @c source and @c destination
/// were either added (if @c is_add is true) or subtracted (if @c is_add is false) and the result was @c result.
/// All other bits will be clear.
template <bool is_add, typename IntT>
static Status::FlagT overflow(IntT source, IntT destination, IntT result) {
	const IntT output_changed = result ^ destination;
	const IntT input_differed = source ^ destination;

	if constexpr (is_add) {
		return top_bit<IntT>() & output_changed & ~input_differed;
	} else {
		return top_bit<IntT>() & output_changed & input_differed;
	}
}

/// Performs an add or subtract (as per @c is_add) between @c source and @c destination,
/// updating @c status. @c is_extend indicates whether this is an extend operation (e.g. ADDX)
/// or a plain one (e.g. ADD).
template <bool is_add, bool is_extend, typename IntT>
static void add_sub(IntT source, IntT &destination, Status &status) {
	static_assert(!std::numeric_limits<IntT>::is_signed);

	const IntT extend = (is_extend && status.extend_flag) ? 1 : 0;
	const IntT result = is_add ?
		(destination + source + extend) :
		(destination - source - extend);

	// Extend operations can reset the zero flag only; non-extend operations
	// can either set it or reset it. Which in the reverse-logic world of
	// zero_result means ORing or storing.
	if constexpr (is_extend) {
		status.zero_result |= Status::FlagT(result);
	} else {
		status.zero_result = Status::FlagT(result);
	}
	status.extend_flag =
	status.carry_flag = is_add ? result < destination : result > destination;
	status.negative_flag = Status::FlagT(result & top_bit<IntT>());
	status.overflow_flag = overflow<is_add>(source, destination, result);
	destination = result;
}

/// Perform an SBCD of @c lhs - @c rhs, storing the result to @c destination and updating @c status.
///
/// @discussion The slightly awkward abandonment of source, destination permits the use of this for both
/// SBCD and NBCD.
inline void sbcd(uint8_t rhs, uint8_t lhs, uint8_t &destination, Status &status) {
	const int extend = (status.extend_flag ? 1 : 0);
	const int unadjusted_result = lhs - rhs - extend;

	const int top = (lhs & 0xf0) - (rhs & 0xf0) - (0x60 & (unadjusted_result >> 4));

	int result = (lhs & 0xf) - (rhs & 0xf) - extend;
	const int low_adjustment = 0x06 & (result >> 4);
	status.extend_flag = status.carry_flag = Status::FlagT(
		(unadjusted_result - low_adjustment) & 0x300
	);
	result = result + top - low_adjustment;

	/* Store the result. */
	destination = uint8_t(result);

	/* Set all remaining flags essentially as if this were normal subtraction. */
	status.zero_result |= destination;
	status.negative_flag = result & 0x80;
	status.overflow_flag = unadjusted_result & ~result & 0x80;
}

/// Perform the bitwise operation defined by @c operation on @c source and @c destination and update @c status.
/// Bitwise operations are any of the byte, word or long versions of AND, OR and EOR.
template <Operation operation, typename IntT>
void bitwise(IntT source, IntT &destination, Status &status) {
	static_assert(
		operation == Operation::ANDb ||	operation == Operation::ANDw || operation == Operation::ANDl ||
		operation == Operation::ORb ||	operation == Operation::ORw || operation == Operation::ORl ||
		operation == Operation::EORb ||	operation == Operation::EORw || operation == Operation::EORl
	);

	switch(operation) {
		case Operation::ANDb:	case Operation::ANDw:	case Operation::ANDl:
			destination &= source;
		break;
		case Operation::ORb:	case Operation::ORw:	case Operation::ORl:
			destination |= source;
		break;
		case Operation::EORb:	case Operation::EORw:	case Operation::EORl:
			destination ^= source;
		break;
	}

	status.overflow_flag = status.carry_flag = 0;
	status.zero_result = destination;
	status.negative_flag = destination & top_bit<IntT>();
}

/// Compare of @c source to @c destination, setting zero, carry, negative and overflow flags.
template <typename IntT>
void compare(IntT source, IntT destination, Status &status) {
	const IntT result = destination - source;
	status.zero_result = result;
	status.carry_flag = result > destination;
	status.negative_flag = result & top_bit<IntT>();
	status.overflow_flag = Primitive::overflow<false>(source, destination, result);
}

/// @returns the name of the bit to be used as a mask for BCLR, BCHG, BSET or BTST for
/// @c instruction given @c source.
inline uint32_t mask_bit(const Preinstruction &instruction, uint32_t source) {
	return source & (instruction.mode<1>() == AddressingMode::DataRegisterDirect ? 31 : 7);
}

/// Perform a BCLR, BCHG or BSET as specified by @c operation and described by @c instruction, @c source and @c destination, updating @c destination and @c status.
/// Also makes an appropriate notification to the @c flow_controller.
template <Operation operation, typename FlowController>
void bit_manipulate(const Preinstruction &instruction, uint32_t source, uint32_t &destination, Status &status, FlowController &flow_controller) {
	static_assert(
		operation == Operation::BCLR ||
		operation == Operation::BCHG ||
		operation == Operation::BSET);

	const auto bit = mask_bit(instruction, source);
	status.zero_result = destination & (1 << bit);
	switch(operation) {
		case Operation::BCLR:	destination &= ~(1 << bit);	break;
		case Operation::BCHG:	destination ^= (1 << bit);	break;
		case Operation::BSET:	destination |= (1 << bit);	break;
	}
	flow_controller.did_bit_op(int(bit));
}

/// Sets @c destination to 0, clears the overflow, carry and negative flags, sets the zero flag.
template <typename IntT> void clear(IntT &destination, Status &status) {
	destination = 0;
	status.negative_flag = status.overflow_flag = status.carry_flag = status.zero_result = 0;
}

/// Perform an ANDI, EORI or ORI to either SR or CCR, notifying @c flow_controller if appropriate.
template <Operation operation, typename FlowController>
void apply_sr_ccr(uint16_t source, Status &status, FlowController &flow_controller) {
	static_assert(
		operation == Operation::ANDItoSR ||	operation == Operation::ANDItoCCR ||
		operation == Operation::EORItoSR ||	operation == Operation::EORItoCCR ||
		operation == Operation::ORItoSR ||	operation == Operation::ORItoCCR
	);

	auto sr = status.status();
	switch(operation) {
		case Operation::ANDItoSR:	case Operation::ANDItoCCR:
			sr &= source;
		break;
		case Operation::EORItoSR:	case Operation::EORItoCCR:
			sr ^= source;
		break;
		case Operation::ORItoSR:	case Operation::ORItoCCR:
			sr |= source;
		break;
	}

	switch(operation) {
		case Operation::ANDItoSR:
		case Operation::EORItoSR:
		case Operation::ORItoSR:
			status.set_status(sr);
			flow_controller.did_update_status();
		break;

		case Operation::ANDItoCCR:
		case Operation::EORItoCCR:
		case Operation::ORItoCCR:
			status.set_ccr(sr);
		break;
	}
}

/// Perform a MULU or MULS between @c source and @c destination, updating @c status and notifying @c flow_controller.
template <bool is_mulu, typename FlowController>
void multiply(uint16_t source, uint32_t &destination, Status &status, FlowController &flow_controller) {
	if constexpr (is_mulu) {
		destination = source * uint16_t(destination);
	} else {
		destination = u_extend16(source) * u_extend16(uint16_t(destination));
	}
	status.carry_flag = status.overflow_flag = 0;
	status.zero_result = destination;
	status.negative_flag = status.zero_result & top_bit<uint32_t>();

	if constexpr (is_mulu) {
		flow_controller.did_mulu(source);
	} else {
		flow_controller.did_muls(source);
	}
}

/// Announce a DIVU or DIVS to @c flow_controller.
template <bool is_divu, bool did_overflow, typename IntT, typename FlowController>
void did_divide(IntT dividend, IntT divisor, FlowController &flow_controller) {
	if constexpr (is_divu) {
		flow_controller.template did_divu<did_overflow>(dividend, divisor);
	} else {
		flow_controller.template did_divs<did_overflow>(dividend, divisor);
	}
}

/// Perform a DIVU or DIVS between @c source and @c destination, updating @c status and notifying @c flow_controller.
template <bool is_divu, typename Int16, typename Int32, typename FlowController>
void divide(uint16_t source, uint32_t &destination, Status &status, FlowController &flow_controller) {
	status.carry_flag = 0;

	const auto dividend = Int32(destination);
	const auto divisor = Int32(Int16(source));

	if(!divisor) {
		status.negative_flag = status.overflow_flag = 0;
		status.zero_result = 1;
		flow_controller.raise_exception(Exception::IntegerDivideByZero);
		did_divide<is_divu, false>(dividend, divisor, flow_controller);
		return;
	}

	const auto quotient = int64_t(dividend) / int64_t(divisor);
	if(quotient != Int32(Int16(quotient))) {
		status.overflow_flag = 1;
		did_divide<is_divu, true>(dividend, divisor, flow_controller);
		return;
	}

	const auto remainder = Int16(dividend % divisor);
	destination = uint32_t((uint32_t(remainder) << 16) | uint16_t(quotient));

	status.overflow_flag = 0;
	status.zero_result = Status::FlagT(quotient);
	status.negative_flag = status.zero_result & 0x8000;
	did_divide<is_divu, false>(dividend, divisor, flow_controller);
}

/// Move @c source to @c destination, updating @c status.
template <typename IntT> void move(IntT source, IntT &destination, Status &status) {
	destination = source;
	status.zero_result = Status::FlagT(source);
	status.negative_flag = status.zero_result & top_bit<IntT>();
	status.overflow_flag = status.carry_flag = 0;
}

/// Perform NEG.[b/l/w] on @c source, updating @c status.
template <bool is_extend, typename IntT> void negative(IntT &source, Status &status) {
	const IntT result = -source - (is_extend && status.extend_flag ? 1 : 0);

	if constexpr (is_extend) {
		status.zero_result |= result;
	} else {
		status.zero_result = result;
	}
	status.extend_flag = status.carry_flag = result;	// i.e. any value other than 0 will result in carry.
	status.negative_flag = result & top_bit<IntT>();
	status.overflow_flag = Primitive::overflow<false>(source, IntT(0), result);

	source = result;
}

/// Perform TST.[b/l/w] with @c source, updating @c status.
template <typename IntT> void test(IntT source, Status &status) {
	status.carry_flag = status.overflow_flag = 0;
	status.zero_result = Status::FlagT(source);
	status.negative_flag = status.zero_result & top_bit<IntT>();
}

/// Decodes the proper shift distance from @c source, notifying the @c flow_controller.
template <typename IntT, typename FlowController> int shift_count(uint8_t source, FlowController &flow_controller) {
	const int count = source & 63;
	flow_controller.template did_shift<IntT>(count);
	return count;
}

/// @returns The number of bits in @c IntT.
template <typename IntT> constexpr int bit_size() {
	return sizeof(IntT) * 8;
}

/// Set the zero and negative flags on @c status according to @c result.
template <typename IntT> void set_neg_zero(IntT result, Status &status) {
	status.zero_result = Status::FlagT(result);
	status.negative_flag = result & top_bit<IntT>();
}

/// Perform an arithmetic or logical shift, i.e. any of LSL, LSR, ASL or ASR.
template <Operation operation, typename IntT, typename FlowController> void shift(uint32_t source, IntT &destination, Status &status, FlowController &flow_controller) {
	static_assert(
		operation == Operation::ASLb || operation == Operation::ASLw || operation == Operation::ASLl ||
		operation == Operation::ASRb || operation == Operation::ASRw || operation == Operation::ASRl ||
		operation == Operation::LSLb || operation == Operation::LSLw || operation == Operation::LSLl ||
		operation == Operation::LSRb || operation == Operation::LSRw || operation == Operation::LSRl
	);

	const auto size = bit_size<IntT>();
	const auto shift = shift_count<IntT>(uint8_t(source), flow_controller);

	if(!shift) {
		status.carry_flag = status.overflow_flag = 0;
	} else {
		enum class Type {
			ASL, LSL, ASR, LSR
		} type;
		switch(operation) {
			case Operation::ASLb:	case Operation::ASLw:	case Operation::ASLl:
				type = Type::ASL;
			break;
			case Operation::LSLb:	case Operation::LSLw:	case Operation::LSLl:
				type = Type::LSL;
			break;
			case Operation::ASRb:	case Operation::ASRw:	case Operation::ASRl:
				type = Type::ASR;
			break;
			case Operation::LSRb:	case Operation::LSRw:	case Operation::LSRl:
				type = Type::LSR;
			break;
		}

		switch(type) {
			case Type::ASL:
			case Type::LSL:
				status.overflow_flag =
					type == Type::LSL ?
						0 : (destination ^ (destination << shift)) & top_bit<IntT>();
				if(shift < size) {
					status.carry_flag = status.extend_flag = (destination >> (size - shift)) & 1;
				} else {
					status.carry_flag = status.extend_flag = 0;
				}
				destination <<= shift;
			break;

			case Type::ASR:
			case Type::LSR: {
				const IntT sign_word =
					type == Type::LSR ?
						0 : (destination & top_bit<IntT>() ? IntT(~0) : 0);

				status.overflow_flag = 0;
				status.carry_flag = status.extend_flag = (destination >> (shift - 1)) & 1;

				if(shift < size) {
					destination = IntT((destination >> shift) | (sign_word << (size - shift)));
				} else {
					destination = sign_word;
				}
			} break;
		}
	}

	set_neg_zero(destination, status);
}

/// Perform a rotate without extend, i.e. any of RO[L/R].[b/w/l].
template <Operation operation, typename IntT, typename FlowController> void rotate(uint32_t source, IntT &destination, Status &status, FlowController &flow_controller) {
	static_assert(
		operation == Operation::ROLb || operation == Operation::ROLw || operation == Operation::ROLl ||
		operation == Operation::RORb || operation == Operation::RORw || operation == Operation::RORl
	);

	const auto size = bit_size<IntT>();
	const auto shift = shift_count<IntT>(uint8_t(source), flow_controller) & (size - 1);

	if(!shift) {
		status.carry_flag = 0;
	} else {
		switch(operation) {
			case Operation::ROLb:	case Operation::ROLw:	case Operation::ROLl:
				destination = IntT(
					(destination << shift) |
					(destination >> (size - shift))
				);
				status.carry_flag = Status::FlagT(destination & 1);
			break;
			case Operation::RORb:	case Operation::RORw:	case Operation::RORl:
				destination = IntT(
					(destination >> shift) |
					(destination << (size - shift))
				);
				status.carry_flag = Status::FlagT(destination & top_bit<IntT>());
			break;
		}
	}

	set_neg_zero(destination, status);
	status.overflow_flag = 0;
}

/// Perform a rotate-through-extend, i.e. any of ROX[L/R].[b/w/l].
template <Operation operation, typename IntT, typename FlowController> void rox(uint32_t source, IntT &destination, Status &status, FlowController &flow_controller) {
	static_assert(
		operation == Operation::ROXLb || operation == Operation::ROXLw || operation == Operation::ROXLl ||
		operation == Operation::ROXRb || operation == Operation::ROXRw || operation == Operation::ROXRl
	);

	const auto size = bit_size<IntT>();
	auto shift = shift_count<IntT>(uint8_t(source), flow_controller) % (size + 1);

	if(!shift) {
		// When shift is zero, extend is unaffected but is copied to carry.
		status.carry_flag = status.extend_flag;
	} else {
 		switch(operation) {
			case Operation::ROXLb:	case Operation::ROXLw:	case Operation::ROXLl:
				status.carry_flag = Status::FlagT((destination >> (size - shift)) & 1);
				destination = IntT(
					(destination << shift) |
					(IntT(status.extend_flag ? 1 : 0) << (shift - 1)) |
					(destination >> (size + 1 - shift))
				);
				status.extend_flag = status.carry_flag;
			break;
			case Operation::ROXRb:	case Operation::ROXRw:	case Operation::ROXRl:
				status.carry_flag = Status::FlagT(destination & (1 << (shift - 1)));
				destination = IntT(
					(destination >> shift) |
					((status.extend_flag ? top_bit<IntT>() : 0) >> (shift - 1)) |
					(destination << (size + 1 - shift))
				);
				status.extend_flag = status.carry_flag;
			break;
		}
	}

	set_neg_zero(destination, status);
	status.overflow_flag = 0;
}

}

template <
	Model model,
	typename FlowController,
	Operation operation = Operation::Undefined
> void perform(Preinstruction instruction, CPU::SlicedInt32 &src, CPU::SlicedInt32 &dest, Status &status, FlowController &flow_controller) {

	switch((operation != Operation::Undefined) ? operation : instruction.operation) {
		/*
			ABCD adds the lowest bytes from the source and destination using BCD arithmetic,
			obeying the extend flag.
		*/
		case Operation::ABCD: {
			// Pull out the two halves, for simplicity.
			const uint8_t source = src.b;
			const uint8_t destination = dest.b;
			const int extend = (status.extend_flag ? 1 : 0);

			// Perform the BCD add by evaluating the two nibbles separately.
			const int unadjusted_result = destination + source + extend;
			int result = (destination & 0xf) + (source & 0xf) + extend;
			result +=
				(destination & 0xf0) +
				(source & 0xf0) +
				(((9 - result) >> 4) & 0x06);			// i.e. ((result > 0x09) ? 0x06 : 0x00)
			result += ((0x9f - result) >> 4) & 0x60;	// i.e. ((result > 0x9f) ? 0x60 : 0x00)

			// Set all flags essentially as if this were normal addition.
			status.zero_result |= result & 0xff;
			status.extend_flag = status.carry_flag = uint_fast32_t(result & ~0xff);
			status.negative_flag = result & 0x80;
			status.overflow_flag = ~unadjusted_result & result & 0x80;

			// Store the result.
			dest.b = uint8_t(result);
		} break;

		// ADD and ADDA add two quantities, the latter sign extending and without setting any flags;
		// ADDQ and SUBQ act as ADD and SUB, but taking the second argument from the instruction code.
		case Operation::ADDb:	Primitive::add_sub<true, false>(src.b, dest.b, status);		break;
		case Operation::SUBb:	Primitive::add_sub<false, false>(src.b, dest.b, status);	break;
		case Operation::ADDXb:	Primitive::add_sub<true, true>(src.b, dest.b, status);		break;
		case Operation::SUBXb:	Primitive::add_sub<false, true>(src.b, dest.b, status);		break;

		case Operation::ADDw:	Primitive::add_sub<true, false>(src.w, dest.w, status);		break;
		case Operation::SUBw:	Primitive::add_sub<false, false>(src.w, dest.w, status);	break;
		case Operation::ADDXw:	Primitive::add_sub<true, true>(src.w, dest.w, status);		break;
		case Operation::SUBXw:	Primitive::add_sub<false, true>(src.w, dest.w, status);		break;

		case Operation::ADDl:	Primitive::add_sub<true, false>(src.l, dest.l, status);		break;
		case Operation::SUBl:	Primitive::add_sub<false, false>(src.l, dest.l, status);	break;
		case Operation::ADDXl:	Primitive::add_sub<true, true>(src.l, dest.l, status);		break;
		case Operation::SUBXl:	Primitive::add_sub<false, true>(src.l, dest.l, status);		break;

		case Operation::ADDAw:	dest.l += u_extend16(src.w);	break;
		case Operation::ADDAl:	dest.l += src.l;				break;
		case Operation::SUBAw:	dest.l -= u_extend16(src.w);	break;
		case Operation::SUBAl:	dest.l -= src.l;				break;

		// BTST/BCLR/etc: modulo for the mask depends on whether memory or a data register is the target.
		case Operation::BTST:
			status.zero_result = dest.l & (1 << Primitive::mask_bit(instruction, src.l));
		break;
		case Operation::BCLR:	Primitive::bit_manipulate<Operation::BCLR>(instruction, src.l, dest.l, status, flow_controller);	break;
		case Operation::BCHG:	Primitive::bit_manipulate<Operation::BCHG>(instruction, src.l, dest.l, status, flow_controller);	break;
		case Operation::BSET:	Primitive::bit_manipulate<Operation::BSET>(instruction, src.l, dest.l, status, flow_controller);	break;

		case Operation::Bccb:
			flow_controller.template complete_bcc<int8_t>(
				status.evaluate_condition(instruction.condition()),
				int8_t(src.b));
		break;

		case Operation::Bccw:
			flow_controller.template complete_bcc<int16_t>(
				status.evaluate_condition(instruction.condition()),
				int16_t(src.w));
		break;

		case Operation::Bccl:
			flow_controller.template complete_bcc<int32_t>(
				status.evaluate_condition(instruction.condition()),
				int32_t(src.l));
		break;

		case Operation::BSRb:
			flow_controller.bsr(uint32_t(int8_t(src.b)));
		break;
		case Operation::BSRw:
			flow_controller.bsr(uint32_t(int16_t(src.w)));
		break;
		case Operation::BSRl:
			flow_controller.bsr(src.l);
		break;

		case Operation::DBcc: {
			const bool matched_condition = status.evaluate_condition(instruction.condition());
			bool overflowed = false;

			// Classify the dbcc.
			if(!matched_condition) {
				-- src.w;
				overflowed = src.w == 0xffff;
			}

			// Take the branch.
			flow_controller.complete_dbcc(
				matched_condition,
				overflowed,
				int16_t(dest.w));
		} break;

		case Operation::Scc: {
			const bool condition = status.evaluate_condition(instruction.condition());
			src.b = condition ? 0xff : 0x00;
			flow_controller.did_scc(condition);
		} break;

		/*
			CLRs: store 0 to the destination, set the zero flag, and clear
			negative, overflow and carry.
		*/
		case Operation::CLRb:	Primitive::clear(src.b, status);	break;
		case Operation::CLRw:	Primitive::clear(src.w, status);	break;
		case Operation::CLRl:	Primitive::clear(src.l, status);	break;

		/*
			CMP.b, CMP.l and CMP.w: sets the condition flags (other than extend) based on a subtraction
			of the source from the destination; the result of the subtraction is not stored.
		*/
		case Operation::CMPb:	Primitive::compare(src.b, dest.b, status);	break;
		case Operation::CMPw:	Primitive::compare(src.w, dest.w, status);	break;
		case Operation::CMPAw:	Primitive::compare(u_extend16(src.w), dest.l, status);	break;
		case Operation::CMPAl:
		case Operation::CMPl:	Primitive::compare(src.l, dest.l, status);	break;

		// JMP: copies EA(0) to the program counter.
		case Operation::JMP:
			flow_controller.jmp(src.l);
		break;

		// JSR: jump to EA(0), pushing the current PC to the stack.
		case Operation::JSR:
			flow_controller.jsr(src.l);
		break;

		/*
			MOVE.b, MOVE.l and MOVE.w: move the least significant byte or word, or the entire long word,
			and set negative, zero, overflow and carry as appropriate.
		*/
		case Operation::MOVEb:	Primitive::move(src.b, dest.b, status);	break;
		case Operation::MOVEw:	Primitive::move(src.w, dest.w, status);	break;
		case Operation::MOVEl:	Primitive::move(src.l, dest.l, status);	break;

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

		case Operation::LEA:
			dest.l = src.l;
		break;

		case Operation::PEA:
			flow_controller.pea(src.l);
		break;

		/*
			Status word moves and manipulations.
		*/

		case Operation::MOVEtoSR:
			status.set_status(src.w);
			flow_controller.did_update_status();
		break;

		case Operation::MOVEfromSR:
			src.w = status.status();
		break;

		case Operation::MOVEtoCCR:
			status.set_ccr(src.w);
		break;

		case Operation::MOVEtoUSP:
			flow_controller.move_to_usp(src.l);
		break;

		case Operation::MOVEfromUSP:
			flow_controller.move_from_usp(src.l);
		break;

		case Operation::EXTbtow:
			src.w = uint16_t(int8_t(src.b));
			status.overflow_flag = status.carry_flag = 0;
			status.zero_result = src.w;
			status.negative_flag = status.zero_result & 0x8000;
		break;

		case Operation::EXTwtol:
			src.l = u_extend16(src.w);
			status.overflow_flag = status.carry_flag = 0;
			status.zero_result = src.l;
			status.negative_flag = status.zero_result & 0x80000000;
		break;

		case Operation::ANDItoSR:	Primitive::apply_sr_ccr<Operation::ANDItoSR>(src.w, status, flow_controller);	break;
		case Operation::EORItoSR:	Primitive::apply_sr_ccr<Operation::EORItoSR>(src.w, status, flow_controller);	break;
		case Operation::ORItoSR:	Primitive::apply_sr_ccr<Operation::ORItoSR>(src.w, status, flow_controller);	break;
		case Operation::ANDItoCCR:	Primitive::apply_sr_ccr<Operation::ANDItoCCR>(src.w, status, flow_controller);	break;
		case Operation::EORItoCCR:	Primitive::apply_sr_ccr<Operation::EORItoCCR>(src.w, status, flow_controller);	break;
		case Operation::ORItoCCR:	Primitive::apply_sr_ccr<Operation::ORItoCCR>(src.w, status, flow_controller);	break;

		/*
			Multiplications.
		*/

		case Operation::MULU:	Primitive::multiply<true>(src.w, dest.l, status, flow_controller);	break;
		case Operation::MULS:	Primitive::multiply<false>(src.w, dest.l, status, flow_controller);	break;

		/*
			Divisions.
		*/

		case Operation::DIVU:	Primitive::divide<true, uint16_t, uint32_t>(src.w, dest.l, status, flow_controller);	break;
		case Operation::DIVS:	Primitive::divide<false, int16_t, int32_t>(src.w, dest.l, status, flow_controller);		break;

		// TRAP, which is a nicer form of ILLEGAL.
		case Operation::TRAP:
			flow_controller.template raise_exception<false>(int(src.l + Exception::TrapBase));
		break;

		case Operation::TRAPV: {
			if(status.overflow_flag) {
				flow_controller.template raise_exception<false>(Exception::TRAPV);
			}
		} break;

		case Operation::CHK: {
			const bool is_under = s_extend16(dest.w) < 0;
			const bool is_over = s_extend16(dest.w) > s_extend16(src.w);

			status.overflow_flag = status.carry_flag = 0;
			status.zero_result = dest.w;

			// Test applied for N:
			//
			//	if Dn < 0, set negative flag;
			//	otherwise, if Dn > <ea>, reset negative flag.
			if(is_over)		status.negative_flag = 0;
			if(is_under)	status.negative_flag = 1;

			// No exception is the default course of action; deviate only if an
			// exception is necessary.
			flow_controller.did_chk(is_under, is_over);
			if(is_under || is_over) {
				flow_controller.template raise_exception<false>(Exception::CHK);
			}
		} break;

		/*
			NEGs: negatives the destination, setting the zero,
			negative, overflow and carry flags appropriate, and extend.

			NB: since the same logic as SUB is used to calculate overflow,
			and SUB calculates `destination - source`, the NEGs deliberately
			label 'source' and 'destination' differently from Motorola.
		*/
		case Operation::NEGb:	Primitive::negative<false>(src.b, status);		break;
		case Operation::NEGw:	Primitive::negative<false>(src.w, status);		break;
		case Operation::NEGl:	Primitive::negative<false>(src.l, status);		break;

		/*
			NEGXs: NEG, with extend.
		*/
		case Operation::NEGXb:	Primitive::negative<true>(src.b, status);		break;
		case Operation::NEGXw:	Primitive::negative<true>(src.w, status);		break;
		case Operation::NEGXl:	Primitive::negative<true>(src.l, status);		break;

		/*
			The no-op.
		*/
		case Operation::NOP:	break;

		/*
			LINK and UNLINK help with stack frames, allowing a certain
			amount of stack space to be allocated or deallocated.
		*/

		case Operation::LINKw:
			flow_controller.link(instruction, uint32_t(int16_t(dest.w)));
		break;

		case Operation::UNLINK:
			flow_controller.unlink(src.l);
		break;

		/*
			TAS: sets zero and negative depending on the current value of the destination,
			and sets the high bit, using a specialised atomic bus cycle.
		*/

		case Operation::TAS:
			flow_controller.tas(instruction, src.l);
		break;

		/*
			Bitwise operators: AND, OR and EOR. All three clear the overflow and carry flags,
			and set zero and negative appropriately.
		*/
		case Operation::ANDb:	Primitive::bitwise<Operation::ANDb>(src.b, dest.b, status);	break;
		case Operation::ANDw:	Primitive::bitwise<Operation::ANDw>(src.w, dest.w, status);	break;
		case Operation::ANDl:	Primitive::bitwise<Operation::ANDl>(src.l, dest.l, status);	break;

		case Operation::ORb:	Primitive::bitwise<Operation::ORb>(src.b, dest.b, status);	break;
		case Operation::ORw:	Primitive::bitwise<Operation::ORw>(src.w, dest.w, status);	break;
		case Operation::ORl:	Primitive::bitwise<Operation::ORl>(src.l, dest.l, status);	break;

		case Operation::EORb:	Primitive::bitwise<Operation::EORb>(src.b, dest.b, status);	break;
		case Operation::EORw:	Primitive::bitwise<Operation::EORw>(src.w, dest.w, status);	break;
		case Operation::EORl:	Primitive::bitwise<Operation::EORl>(src.l, dest.l, status);	break;

		// NOTs: take the logical inverse, affecting the negative and zero flags.
		case Operation::NOTb:
			src.b ^= 0xff;
			status.zero_result = src.b;
			status.negative_flag = status.zero_result & 0x80;
			status.overflow_flag = status.carry_flag = 0;
		break;

		case Operation::NOTw:
			src.w ^= 0xffff;
			status.zero_result = src.w;
			status.negative_flag = status.zero_result & 0x8000;
			status.overflow_flag = status.carry_flag = 0;
		break;

		case Operation::NOTl:
			src.l ^= 0xffffffff;
			status.zero_result = src.l;
			status.negative_flag = status.zero_result & 0x80000000;
			status.overflow_flag = status.carry_flag = 0;
		break;

		/*
			SBCD subtracts the lowest byte of the source from that of the destination using
			BCD arithmetic, obeying the extend flag.
		*/
		case Operation::SBCD:
			Primitive::sbcd(src.b, dest.b, dest.b, status);
		break;

		/*
			NBCD is like SBCD except that the result is 0 - source rather than
			destination - source.
		*/
		case Operation::NBCD:
			Primitive::sbcd(src.b, 0, src.b, status);
		break;

		// EXG and SWAP exchange/swap words or long words.

		case Operation::EXG: {
			const auto temporary = src.l;
			src.l = dest.l;
			dest.l = temporary;
		} break;

		case Operation::SWAP: {
			uint16_t *const words = reinterpret_cast<uint16_t *>(&src.l);
			const auto temporary = words[0];
			words[0] = words[1];
			words[1] = temporary;

			status.zero_result = src.l;
			status.negative_flag = temporary & 0x8000;
			status.overflow_flag = status.carry_flag = 0;
		} break;

		/*
			Shifts and rotates.
		*/
		case Operation::ASLm:
			status.extend_flag = status.carry_flag = src.w & Primitive::top_bit<uint16_t>();
			status.overflow_flag = (src.w ^ (src.w << 1)) & Primitive::top_bit<uint16_t>();
			src.w <<= 1;
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::LSLm:
			status.extend_flag = status.carry_flag = src.w & Primitive::top_bit<uint16_t>();
			status.overflow_flag = 0;
			src.w <<= 1;
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::ASRm:
			status.extend_flag = status.carry_flag = src.w & 1;
			status.overflow_flag = 0;
			src.w = (src.w & Primitive::top_bit<uint16_t>()) | (src.w >> 1);
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::LSRm:
			status.extend_flag = status.carry_flag = src.w & 1;
			status.overflow_flag = 0;
			src.w >>= 1;
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::ROLm:
			src.w = uint16_t((src.w << 1) | (src.w >> 15));
			status.carry_flag = src.w & 0x0001;
			status.overflow_flag = 0;
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::RORm:
			src.w = uint16_t((src.w >> 1) | (src.w << 15));
			status.carry_flag = src.w & Primitive::top_bit<uint16_t>();
			status.overflow_flag = 0;
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::ROXLm:
			status.carry_flag = src.w & Primitive::top_bit<uint16_t>();
			src.w = uint16_t((src.w << 1) | (status.extend_flag ? 0x0001 : 0x0000));
			status.extend_flag = status.carry_flag;
			status.overflow_flag = 0;
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::ROXRm:
			status.carry_flag = src.w & 0x0001;
			src.w = uint16_t((src.w >> 1) | (status.extend_flag ? 0x8000 : 0x0000));
			status.extend_flag = status.carry_flag;
			status.overflow_flag = 0;
			Primitive::set_neg_zero(src.w, status);
		break;

		case Operation::ASLb:	Primitive::shift<Operation::ASLb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::ASLw:	Primitive::shift<Operation::ASLw>(src.l, dest.w, status, flow_controller);	break;
		case Operation::ASLl:	Primitive::shift<Operation::ASLl>(src.l, dest.l, status, flow_controller);	break;

		case Operation::ASRb:	Primitive::shift<Operation::ASRb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::ASRw:	Primitive::shift<Operation::ASRw>(src.l, dest.w, status, flow_controller);	break;
		case Operation::ASRl:	Primitive::shift<Operation::ASRl>(src.l, dest.l, status, flow_controller);	break;

		case Operation::LSLb:	Primitive::shift<Operation::LSLb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::LSLw:	Primitive::shift<Operation::LSLw>(src.l, dest.w, status, flow_controller);	break;
		case Operation::LSLl:	Primitive::shift<Operation::LSLl>(src.l, dest.l, status, flow_controller);	break;

		case Operation::LSRb:	Primitive::shift<Operation::LSRb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::LSRw:	Primitive::shift<Operation::LSRw>(src.l, dest.w, status, flow_controller);	break;
		case Operation::LSRl:	Primitive::shift<Operation::LSRl>(src.l, dest.l, status, flow_controller);	break;

		case Operation::ROLb:	Primitive::rotate<Operation::ROLb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::ROLw:	Primitive::rotate<Operation::ROLw>(src.l, dest.w, status, flow_controller); break;
		case Operation::ROLl:	Primitive::rotate<Operation::ROLl>(src.l, dest.l, status, flow_controller); break;

		case Operation::RORb:	Primitive::rotate<Operation::RORb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::RORw:	Primitive::rotate<Operation::RORw>(src.l, dest.w, status, flow_controller); break;
		case Operation::RORl:	Primitive::rotate<Operation::RORl>(src.l, dest.l, status, flow_controller); break;

		case Operation::ROXLb:	Primitive::rox<Operation::ROXLb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::ROXLw:	Primitive::rox<Operation::ROXLw>(src.l, dest.w, status, flow_controller);	break;
		case Operation::ROXLl:	Primitive::rox<Operation::ROXLl>(src.l, dest.l, status, flow_controller);	break;

		case Operation::ROXRb:	Primitive::rox<Operation::ROXRb>(src.l, dest.b, status, flow_controller);	break;
		case Operation::ROXRw:	Primitive::rox<Operation::ROXRw>(src.l, dest.w, status, flow_controller);	break;
		case Operation::ROXRl:	Primitive::rox<Operation::ROXRl>(src.l, dest.l, status, flow_controller);	break;

		case Operation::MOVEPl:
			flow_controller.template movep<uint32_t>(instruction, src.l, dest.l);
		break;

		case Operation::MOVEPw:
			flow_controller.template movep<uint16_t>(instruction, src.l, dest.l);
		break;

		case Operation::MOVEMtoRl:
			flow_controller.template movem_toR<uint32_t>(instruction, src.l, dest.l);
		break;

		case Operation::MOVEMtoMl:
			flow_controller.template movem_toM<uint32_t>(instruction, src.l, dest.l);
		break;

		case Operation::MOVEMtoRw:
			flow_controller.template movem_toR<uint16_t>(instruction, src.l, dest.l);
		break;

		case Operation::MOVEMtoMw:
			flow_controller.template movem_toM<uint16_t>(instruction, src.l, dest.l);
		break;

		/*
			RTE and RTR share an implementation.
		*/
		case Operation::RTR:
			flow_controller.rtr();
		break;

		case Operation::RTE:
			flow_controller.rte();
		break;

		case Operation::RTS:
			flow_controller.rts();
		break;

		/*
			TSTs: compare to zero.
		*/

		case Operation::TSTb:	Primitive::test(src.b, status);	break;
		case Operation::TSTw:	Primitive::test(src.w, status);	break;
		case Operation::TSTl:	Primitive::test(src.l, status);	break;

		case Operation::STOP:
			status.set_status(src.w);
			flow_controller.did_update_status();
			flow_controller.stop();
		break;

		case Operation::RESET:
			flow_controller.reset();
		break;

		/*
			Development period debugging.
		*/
		default:
			assert(false);
		break;
	}

}

}
}

#endif /* InstructionSets_M68k_PerformImplementation_h */
