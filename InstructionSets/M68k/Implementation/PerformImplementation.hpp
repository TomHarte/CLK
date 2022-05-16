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

#define u_extend16(x)	uint32_t(int16_t(x))
#define s_extend16(x)	int32_t(int16_t(x))

template <
	Model model,
	typename FlowController,
	Operation operation = Operation::Undefined
> void perform(Preinstruction instruction, CPU::SlicedInt32 &src, CPU::SlicedInt32 &dest, Status &status, FlowController &flow_controller) {

#define sub_overflow() ((result ^ destination) & (destination ^ source))
#define add_overflow() ((result ^ destination) & ~(destination ^ source))
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
	zero_op(status.zero_result, b);	\
	status.extend_flag = status.carry_flag = uint_fast32_t(result & ~0xff);	\
	status.negative_flag = result & 0x80;	\
	status.overflow_flag = overflow() & 0x80;

#define addsubw(a, b, op, overflow, x, zero_op)	\
	const int source = a;	\
	const int destination = b;	\
	const auto result = op(destination, source, x);	\
	\
	b = uint16_t(result);	\
	zero_op(status.zero_result, b);	\
	status.extend_flag = status.carry_flag = uint_fast32_t(result & ~0xffff);	\
	status.negative_flag = result & 0x8000;	\
	status.overflow_flag = overflow() & 0x8000;

#define addsubl(a, b, op, overflow, x, zero_op)	\
	const uint64_t source = a;	\
	const uint64_t destination = b;	\
	const auto result = op(destination, source, x);	\
	\
	b = uint32_t(result);	\
	zero_op(status.zero_result, b);	\
	status.extend_flag = status.carry_flag = uint_fast32_t(result >> 32);	\
	status.negative_flag = result & 0x80000000;	\
	status.overflow_flag = overflow() & 0x80000000;

#define addb(a, b, x, z) addsubb(a, b, addop, add_overflow, x, z)
#define subb(a, b, x, z) addsubb(a, b, subop, sub_overflow, x, z)
#define addw(a, b, x, z) addsubw(a, b, addop, add_overflow, x, z)
#define subw(a, b, x, z) addsubw(a, b, subop, sub_overflow, x, z)
#define addl(a, b, x, z) addsubl(a, b, addop, add_overflow, x, z)
#define subl(a, b, x, z) addsubl(a, b, subop, sub_overflow, x, z)

#define no_extend(op, a, b)	op(a, b, 0, z_set)
#define extend(op, a, b)	op(a, b, status.extend_flag, z_or)

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

#define get_mask()																						\
	const uint32_t mask_size = (instruction.mode<1>() == AddressingMode::DataRegisterDirect) ? 31 : 7;	\
	const uint32_t bit_position = src.l & mask_size;													\
	const uint32_t bit_mask = 1 << bit_position

		// BTST/BCLR/etc: modulo for the mask depends on whether memory or a data register is the target.
		case Operation::BTST: {
			get_mask();
			status.zero_result = dest.l & bit_mask;
		} break;

		case Operation::BCLR: {
			get_mask();

			status.zero_result = dest.l & bit_mask;
			dest.l &= ~bit_mask;
			flow_controller.did_bit_op(bit_position);
		} break;

		case Operation::BCHG: {
			get_mask();

			status.zero_result = dest.l & bit_mask;
			dest.l ^= bit_mask;
			flow_controller.did_bit_op(bit_position);
		} break;

		case Operation::BSET: {
			get_mask();

			status.zero_result = dest.l & bit_mask;
			dest.l |= bit_mask;
			flow_controller.did_bit_op(bit_position);
		} break;

#undef get_mask

		case Operation::Bccb:
			flow_controller.template complete_bcc<int8_t>(
				status.evaluate_condition(instruction.condition()),
				src.b);
		break;

		case Operation::Bccw:
			flow_controller.template complete_bcc<int16_t>(
				status.evaluate_condition(instruction.condition()),
				src.w);
		break;

		case Operation::Bccl:
			flow_controller.template complete_bcc<int32_t>(
				status.evaluate_condition(instruction.condition()),
				src.l);
		break;

		case Operation::BSRb:
			flow_controller.bsr(int8_t(src.b) + 2);
		break;
		case Operation::BSRw:
			flow_controller.bsr(int16_t(src.w) + 2);
		break;
		case Operation::BSRl:
			flow_controller.bsr(src.l + 2);
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

		case Operation::Scc:
			src.b = status.evaluate_condition(instruction.condition()) ? 0xff : 0x00;
		break;

		/*
			CLRs: store 0 to the destination, set the zero flag, and clear
			negative, overflow and carry.
		*/
		case Operation::CLRb:
			src.b = 0;
			status.negative_flag = status.overflow_flag = status.carry_flag = status.zero_result = 0;
		break;

		case Operation::CLRw:
			src.w = 0;
			status.negative_flag = status.overflow_flag = status.carry_flag = status.zero_result = 0;
		break;

		case Operation::CLRl:
			src.l = 0;
			status.negative_flag = status.overflow_flag = status.carry_flag = status.zero_result = 0;
		break;

		/*
			CMP.b, CMP.l and CMP.w: sets the condition flags (other than extend) based on a subtraction
			of the source from the destination; the result of the subtraction is not stored.
		*/
		case Operation::CMPb: {
			const uint8_t source = src.b;
			const uint8_t destination = dest.b;
			const int result = destination - source;

			status.zero_result = result & 0xff;
			status.carry_flag = Status::FlagT(result & ~0xff);
			status.negative_flag = result & 0x80;
			status.overflow_flag = sub_overflow() & 0x80;
		} break;

		case Operation::CMPw: {
			const uint16_t source = src.w;
			const uint16_t destination = dest.w;
			const int result = destination - source;

			status.zero_result = result & 0xffff;
			status.carry_flag = Status::FlagT(result & ~0xffff);
			status.negative_flag = result & 0x8000;
			status.overflow_flag = sub_overflow() & 0x8000;
		} break;

		case Operation::CMPAw: {
			const auto source = uint64_t(u_extend16(src.w));
			const uint64_t destination = dest.l;
			const auto result = destination - source;

			status.zero_result = uint32_t(result);
			status.carry_flag = result >> 32;
			status.negative_flag = result & 0x80000000;
			status.overflow_flag = sub_overflow() & 0x80000000;
		} break;

		// TODO: is there any benefit to keeping both of these?
		case Operation::CMPAl:
		case Operation::CMPl: {
			const auto source = uint64_t(src.l);
			const auto destination = uint64_t(dest.l);
			const auto result = destination - source;

			status.zero_result = uint32_t(result);
			status.carry_flag = result >> 32;
			status.negative_flag = result & 0x80000000;
			status.overflow_flag = sub_overflow() & 0x80000000;
		} break;

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
		case Operation::MOVEb:
			status.zero_result = dest.b = src.b;
			status.negative_flag = status.zero_result & 0x80;
			status.overflow_flag = status.carry_flag = 0;
		break;

		case Operation::MOVEw:
			status.zero_result = dest.w = src.w;
			status.negative_flag = status.zero_result & 0x8000;
			status.overflow_flag = status.carry_flag = 0;
		break;

		case Operation::MOVEl:
			status.zero_result = dest.l = src.l;
			status.negative_flag = status.zero_result & 0x80000000;
			status.overflow_flag = status.carry_flag = 0;
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

#define and_op(a, b) a &= b
#define or_op(a, b) a |= b
#define eor_op(a, b) a ^= b

#define apply(op, func)	{			\
	auto sr = status.status();		\
	op(sr, src.w);					\
	func(sr);						\
}

#define set_status(x)		status.set_status(x);	flow_controller.did_update_status()
#define set_ccr(x)			status.set_ccr(x)

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
#undef set_ccr
#undef set_status
#undef apply
#undef eor_op
#undef or_op
#undef and_op

		/*
			Multiplications.
		*/

		case Operation::MULU:
			dest.l = dest.w * src.w;
			status.carry_flag = status.overflow_flag = 0;
			status.zero_result = dest.l;
			status.negative_flag = status.zero_result & 0x80000000;
			flow_controller.did_mulu(src.w);
		break;

		case Operation::MULS:
			dest.l =
				u_extend16(dest.w) * u_extend16(src.w);
			status.carry_flag = status.overflow_flag = 0;
			status.zero_result = dest.l;
			status.negative_flag = status.zero_result & 0x80000000;
			flow_controller.did_muls(src.w);
		break;

		/*
			Divisions.
		*/

#define DIV(Type16, Type32, flow_function) {								\
	status.carry_flag = 0;													\
																			\
	if(!src.w) {															\
		status.negative_flag = status.overflow_flag = 0;					\
		status.zero_result = 1;												\
		flow_controller.raise_exception(Exception::IntegerDivideByZero);	\
		return;																\
	}																		\
																			\
	const auto dividend = Type32(dest.l);									\
	const auto divisor = Type32(Type16(src.w));								\
	const auto quotient = dividend / divisor;								\
																			\
	if(quotient != Type32(Type16(quotient))) {								\
		status.overflow_flag = 1;											\
		flow_controller.template flow_function<true>(dividend, divisor);	\
		return;																\
	}																		\
																			\
	const auto remainder = Type16(dividend % divisor);						\
	dest.l = uint32_t((uint32_t(remainder) << 16) | uint16_t(quotient));	\
																			\
	status.overflow_flag = 0;												\
	status.zero_result = Status::FlagT(quotient);							\
	status.negative_flag = status.zero_result & 0x8000;						\
	flow_controller.template flow_function<false>(dividend, divisor);		\
}

		case Operation::DIVU:	DIV(uint16_t, uint32_t, did_divu);	break;
		case Operation::DIVS: 	DIV(int16_t, int32_t, did_divs);	break;

#undef DIV

		// TRAP, which is a nicer form of ILLEGAL.
		case Operation::TRAP:
			flow_controller.template raise_exception<false>(src.l + Exception::TrapBase);
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
		case Operation::NEGb: {
			const int destination = 0;
			const int source = src.b;
			const auto result = destination - source;
			src.b = uint8_t(result);

			status.zero_result = result & 0xff;
			status.extend_flag = status.carry_flag = Status::FlagT(result & ~0xff);
			status.negative_flag = result & 0x80;
			status.overflow_flag = sub_overflow() & 0x80;
		} break;

		case Operation::NEGw: {
			const int destination = 0;
			const int source = src.w;
			const auto result = destination - source;
			src.w = uint16_t(result);

			status.zero_result = result & 0xffff;
			status.extend_flag = status.carry_flag = Status::FlagT(result & ~0xffff);
			status.negative_flag = result & 0x8000;
			status.overflow_flag = sub_overflow() & 0x8000;
		} break;

		case Operation::NEGl: {
			const uint64_t destination = 0;
			const uint64_t source = src.l;
			const auto result = destination - source;
			src.l = uint32_t(result);

			status.zero_result = uint_fast32_t(result);
			status.extend_flag = status.carry_flag = result >> 32;
			status.negative_flag = result & 0x80000000;
			status.overflow_flag = sub_overflow() & 0x80000000;
		} break;

		/*
			NEGXs: NEG, with extend.
		*/
		case Operation::NEGXb: {
			const int source = src.b;
			const int destination = 0;
			const auto result = destination - source - (status.extend_flag ? 1 : 0);
			src.b = uint8_t(result);

			status.zero_result |= result & 0xff;
			status.extend_flag = status.carry_flag = Status::FlagT(result & ~0xff);
			status.negative_flag = result & 0x80;
			status.overflow_flag = sub_overflow() & 0x80;
		} break;

		case Operation::NEGXw: {
			const int source = src.w;
			const int destination = 0;
			const auto result = destination - source - (status.extend_flag ? 1 : 0);
			src.w = uint16_t(result);

			status.zero_result |= result & 0xffff;
			status.extend_flag = status.carry_flag = Status::FlagT(result & ~0xffff);
			status.negative_flag = result & 0x8000;
			status.overflow_flag = sub_overflow() & 0x8000;
		} break;

		case Operation::NEGXl: {
			const uint64_t source = src.l;
			const uint64_t destination = 0;
			const auto result = destination - source - (status.extend_flag ? 1 : 0);
			src.l = uint32_t(result);

			status.zero_result |= uint_fast32_t(result);
			status.extend_flag = status.carry_flag = result >> 32;
			status.negative_flag = result & 0x80000000;
			status.overflow_flag = sub_overflow() & 0x80000000;
		} break;

		/*
			The no-op.
		*/
		case Operation::NOP:	break;

		/*
			LINK and UNLINK help with stack frames, allowing a certain
			amount of stack space to be allocated or deallocated.
		*/

		case Operation::LINKw:
			flow_controller.link(instruction, int16_t(dest.w));
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
#define op_and(x, y)	x &= y
#define op_or(x, y)		x |= y
#define op_eor(x, y)	x ^= y

#define bitwise(source, dest, sign_mask, operator)	\
	operator(dest, source);							\
	status.overflow_flag = status.carry_flag = 0;	\
	status.zero_result = dest;						\
	status.negative_flag = dest & sign_mask;

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

#define sbcd(d)																					\
	const int extend = (status.extend_flag ? 1 : 0);											\
	const int unadjusted_result = destination - source - extend;								\
																								\
	const int top = (destination & 0xf0) - (source & 0xf0) - (0x60 & (unadjusted_result >> 4));	\
																								\
	int result = (destination & 0xf) - (source & 0xf) - extend;									\
	const int low_adjustment = 0x06 & (result >> 4);											\
	status.extend_flag = status.carry_flag = Status::FlagT(										\
		(unadjusted_result - low_adjustment) & 0x300											\
	);																							\
	result = result + top - low_adjustment;														\
																								\
	/* Store the result. */																		\
	d = uint8_t(result);																		\
																								\
	/* Set all remaining flags essentially as if this were normal subtraction. */				\
	status.zero_result |= d;														\
	status.negative_flag = result & 0x80;														\
	status.overflow_flag = unadjusted_result & ~result & 0x80;									\

		/*
			SBCD subtracts the lowest byte of the source from that of the destination using
			BCD arithmetic, obeying the extend flag.
		*/
		case Operation::SBCD: {
			const uint8_t source = src.b;
			const uint8_t destination = dest.b;
			sbcd(dest.b);
		} break;

		/*
			NBCD is like SBCD except that the result is 0 - source rather than
			destination - source.
		*/
		case Operation::NBCD: {
			const uint8_t source = src.b;
			const uint8_t destination = 0;
			sbcd(src.b);
		} break;

#undef sbcd

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
#define set_neg_zero(v, m)														\
	status.zero_result = Status::FlagT(v);						\
	status.negative_flag = status.zero_result & Status::FlagT(m);

#define set_neg_zero_overflow(v, m)									\
	set_neg_zero(v, m);												\
	status.overflow_flag = (Status::FlagT(value) ^ status.zero_result) & Status::FlagT(m);

#define decode_shift_count()	\
	int shift_count = src.l & 63;	\
	flow_controller.did_shift(shift_count);

#define set_flags_w(t) set_flags(src.w, 0x8000, t)

#define asl(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag = status.overflow_flag = 0;	\
	} else {	\
		destination = (shift_count < size) ? decltype(destination)(value << shift_count) : 0;	\
		status.extend_flag = status.carry_flag = Status::FlagT(value) & Status::FlagT( (1u << (size - 1)) >> (shift_count - 1) );	\
		\
		if(shift_count >= size) status.overflow_flag = value && (value != decltype(value)(-1));	\
		else {	\
			const auto mask = decltype(destination)(0xffffffff << (size - shift_count));	\
			status.overflow_flag = mask & value && ((mask & value) != mask);	\
		}	\
	}	\
	\
	set_neg_zero(destination, 1 << (size - 1));	\
}

		case Operation::ASLm: {
			const auto value = src.w;
			src.w = uint16_t(value << 1);
			status.extend_flag = status.carry_flag = value & 0x8000;
			set_neg_zero_overflow(src.w, 0x8000);
		} break;
		case Operation::ASLb: asl(dest.b, 8);	break;
		case Operation::ASLw: asl(dest.w, 16); 	break;
		case Operation::ASLl: asl(dest.l, 32); 	break;

#define asr(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag = 0;	\
	} else {	\
		destination = (shift_count < size) ?	\
		decltype(destination)(\
			(value >> shift_count) |	\
			((value & decltype(value)(1 << (size - 1)) ? 0xffffffff : 0x000000000) << (size - shift_count))	\
		) :	\
			decltype(destination)(	\
			(value & decltype(value)(1 << (size - 1))) ? 0xffffffff : 0x000000000	\
		);	\
		status.extend_flag = status.carry_flag = Status::FlagT(value) & Status::FlagT(1 << (shift_count - 1));	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ASRm: {
			const auto value = src.w;
			src.w = (value&0x8000) | (value >> 1);
			status.extend_flag = status.carry_flag = value & 1;
			set_neg_zero_overflow(src.w, 0x8000);
		} break;
		case Operation::ASRb: asr(dest.b, 8);	break;
		case Operation::ASRw: asr(dest.w, 16); 	break;
		case Operation::ASRl: asr(dest.l, 32); 	break;


#undef set_neg_zero_overflow
#define set_neg_zero_overflow(v, m)	\
	set_neg_zero(v, m);	\
	status.overflow_flag = 0;

#undef set_flags
#define set_flags(v, m, t)	\
	status.zero_result = v;	\
	status.negative_flag = status.zero_result & (m);	\
	status.overflow_flag = 0;	\
	status.carry_flag = value & (t);

#define lsl(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag = 0;	\
	} else {	\
		destination = (shift_count < size) ? decltype(destination)(value << shift_count) : 0;	\
		status.extend_flag = status.carry_flag = Status::FlagT(value) & Status::FlagT( (1u << (size - 1)) >> (shift_count - 1) );	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::LSLm: {
			const auto value = src.w;
			src.w = uint16_t(value << 1);
			status.extend_flag = status.carry_flag = value & 0x8000;
			set_neg_zero_overflow(src.w, 0x8000);
		} break;
		case Operation::LSLb: lsl(dest.b, 8);	break;
		case Operation::LSLw: lsl(dest.w, 16); 	break;
		case Operation::LSLl: lsl(dest.l, 32); 	break;

#define lsr(destination, size)	{\
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag = 0;	\
	} else {	\
		destination = (shift_count < size) ? (value >> shift_count) : 0;	\
		status.extend_flag = status.carry_flag = value & Status::FlagT(1 << (shift_count - 1));	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::LSRm: {
			const auto value = src.w;
			src.w = value >> 1;
			status.extend_flag = status.carry_flag = value & 1;
			set_neg_zero_overflow(src.w, 0x8000);
		} break;
		case Operation::LSRb: lsr(dest.b, 8);	break;
		case Operation::LSRw: lsr(dest.w, 16); 	break;
		case Operation::LSRl: lsr(dest.l, 32); 	break;

#define rol(destination, size)	{ \
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag = 0;	\
	} else {	\
		shift_count &= (size - 1);	\
		destination = decltype(destination)(	\
			(value << shift_count) |	\
			(value >> (size - shift_count))	\
		);	\
		status.carry_flag = Status::FlagT(destination & 1);	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ROLm: {
			const auto value = src.w;
			src.w = uint16_t((value << 1) | (value >> 15));
			status.carry_flag = src.w & 1;
			set_neg_zero_overflow(src.w, 0x8000);
		} break;
		case Operation::ROLb: rol(dest.b, 8);	break;
		case Operation::ROLw: rol(dest.w, 16); 	break;
		case Operation::ROLl: rol(dest.l, 32); 	break;

#define ror(destination, size)	{ \
	decode_shift_count();	\
	const auto value = destination;	\
	\
	if(!shift_count) {	\
		status.carry_flag = 0;	\
	} else {	\
		shift_count &= (size - 1);	\
		destination = decltype(destination)(\
			(value >> shift_count) |	\
			(value << (size - shift_count))	\
		);\
		status.carry_flag = destination & Status::FlagT(1 << (size - 1));	\
	}	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::RORm: {
			const auto value = src.w;
			src.w = uint16_t((value >> 1) | (value << 15));
			status.carry_flag = src.w & 0x8000;
			set_neg_zero_overflow(src.w, 0x8000);
		} break;
		case Operation::RORb: ror(dest.b, 8);	break;
		case Operation::RORw: ror(dest.w, 16); 	break;
		case Operation::RORl: ror(dest.l, 32); 	break;

#define roxl(destination, size)	{ \
	decode_shift_count();	\
	\
	shift_count %= (size + 1);	\
	uint64_t compound = uint64_t(destination) | (status.extend_flag ? (1ull << size) : 0);	\
	compound = \
		(compound << shift_count) |	\
		(compound >> (size + 1 - shift_count));	\
	status.carry_flag = status.extend_flag = Status::FlagT((compound >> size) & 1);	\
	destination = decltype(destination)(compound);	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ROXLm: {
			const auto value = src.w;
			src.w = uint16_t((value << 1) | (status.extend_flag ? 0x0001 : 0x0000));
			status.extend_flag = value & 0x8000;
			set_flags_w(0x8000);
		} break;
		case Operation::ROXLb: roxl(dest.b, 8);		break;
		case Operation::ROXLw: roxl(dest.w, 16); 	break;
		case Operation::ROXLl: roxl(dest.l, 32); 	break;

#define roxr(destination, size)	{ \
	decode_shift_count();	\
	\
	shift_count %= (size + 1);	\
	uint64_t compound = uint64_t(destination) | (status.extend_flag ? (1ull << size) : 0);	\
	compound = \
		(compound >> shift_count) |	\
		(compound << (size + 1 - shift_count));	\
		status.carry_flag = status.extend_flag = Status::FlagT((compound >> size) & 1);	\
	destination = decltype(destination)(compound);	\
	\
	set_neg_zero_overflow(destination, 1 << (size - 1));	\
}

		case Operation::ROXRm: {
			const auto value = src.w;
			src.w = (value >> 1) | (status.extend_flag ? 0x8000 : 0x0000);
			status.extend_flag = value & 0x0001;
			set_flags_w(0x0001);
		} break;
		case Operation::ROXRb: roxr(dest.b, 8);		break;
		case Operation::ROXRw: roxr(dest.w, 16); 	break;
		case Operation::ROXRl: roxr(dest.l, 32); 	break;

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
#undef set_flags_w
#undef set_neg_zero_overflow
#undef set_neg_zero

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

		case Operation::TSTb:
			status.carry_flag = status.overflow_flag = 0;
			status.zero_result = src.b;
			status.negative_flag = status.zero_result & 0x80;
		break;

		case Operation::TSTw:
			status.carry_flag = status.overflow_flag = 0;
			status.zero_result = src.w;
			status.negative_flag = status.zero_result & 0x8000;
		break;

		case Operation::TSTl:
			status.carry_flag = status.overflow_flag = 0;
			status.zero_result = src.l;
			status.negative_flag = status.zero_result & 0x80000000;
		break;

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
#undef sub_overflow
#undef add_overflow

#undef u_extend16
#undef s_extend16

}

}
}

#endif /* InstructionSets_M68k_PerformImplementation_h */
