//
//  ShiftRoll.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef ShiftRoll_hpp
#define ShiftRoll_hpp

#include "../AccessType.hpp"

namespace InstructionSet::x86::Primitive {

template <typename IntT, typename ContextT>
void rcl(
	modify_t<IntT> destination,
	uint8_t count,
	ContextT &context
) {
	/*
		(* RCL and RCR instructions *)
		SIZE ← OperandSize
		CASE (determine count) OF
			SIZE = 8:	tempCOUNT ← (COUNT AND 1FH) MOD 9;
			SIZE = 16:	tempCOUNT ← (COUNT AND 1FH) MOD 17;
			SIZE = 32:	tempCOUNT ← COUNT AND 1FH;
		ESAC;
	*/
	/*
		(* RCL instruction operation *)
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← MSB(DEST);
				DEST ← (DEST * 2) + CF;
				CF ← tempCF;
				tempCOUNT ← tempCOUNT – 1;
			OD;
		ELIHW;
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR CF;
			ELSE OF is undefined;
		FI;
	*/
	/*
		The CF flag contains the value of the bit shifted into it.
		The OF flag is affected only for single- bit rotates (see “Description” above);
		it is undefined for multi-bit rotates. The SF, ZF, AF, and PF flags are not affected.
	*/
	const auto temp_count = count % (Numeric::bit_size<IntT>() + 1);
	auto carry = context.flags.template carry_bit<IntT>();
	switch(temp_count) {
		case 0: break;
		case Numeric::bit_size<IntT>(): {
			const IntT temp_carry = destination & 1;
			destination = (destination >> 1) | (carry << (Numeric::bit_size<IntT>() - 1));
			carry = temp_carry;
		} break;
		default: {
			const IntT temp_carry = destination & (Numeric::top_bit<IntT>() >> (temp_count - 1));
			destination =
				(destination << temp_count) |
				(destination >> (Numeric::bit_size<IntT>() + 1 - temp_count)) |
				(carry << (temp_count - 1));
			carry = temp_carry ? 1 : 0;
		} break;
	}

	context.flags.template set_from<Flag::Carry>(carry);
	context.flags.template set_from<Flag::Overflow>(
		((destination >> (Numeric::bit_size<IntT>() - 1)) & 1) ^ carry
	);
}

template <typename IntT, typename ContextT>
void rcr(
	modify_t<IntT> destination,
	uint8_t count,
	ContextT &context
) {
	/*
		(* RCR instruction operation *)
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR CF;
			ELSE OF is undefined;
		FI;
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← LSB(SRC);
				DEST ← (DEST / 2) + (CF * 2SIZE);
				CF ← tempCF;
				tempCOUNT ← tempCOUNT – 1;
			OD;
	*/
	auto carry = context.flags.template carry_bit<IntT>();
	context.flags.template set_from<Flag::Overflow>(
		((destination >> (Numeric::bit_size<IntT>() - 1)) & 1) ^ carry
	);

	const auto temp_count = count % (Numeric::bit_size<IntT>() + 1);
	switch(temp_count) {
		case 0: break;
		case Numeric::bit_size<IntT>(): {
			const IntT temp_carry = destination & Numeric::top_bit<IntT>();
			destination = (destination << 1) | carry;
			carry = temp_carry;
		} break;
		default: {
			const IntT temp_carry = destination & (1 << (temp_count - 1));
			destination =
				(destination >> temp_count) |
				(destination << (Numeric::bit_size<IntT>() + 1 - temp_count)) |
				(carry << (Numeric::bit_size<IntT>() - temp_count));
			carry = temp_carry;
		} break;
	}

	context.flags.template set_from<Flag::Carry>(carry);
}

template <typename IntT, typename ContextT>
void rol(
	modify_t<IntT> destination,
	uint8_t count,
	ContextT &context
) {
	/*
		(* ROL and ROR instructions *)
		SIZE ← OperandSize
		CASE (determine count) OF
			SIZE = 8:	tempCOUNT ← COUNT MOD 8;
			SIZE = 16:	tempCOUNT ← COUNT MOD 16;
			SIZE = 32:	tempCOUNT ← COUNT MOD 32;
		ESAC;
	*/
	/*
		(* ROL instruction operation *)
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← MSB(DEST);
				DEST ← (DEST * 2) + tempCF;
				tempCOUNT ← tempCOUNT – 1;
			OD;
		ELIHW;
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR CF;
			ELSE OF is undefined;
		FI;
	*/
	/*
		The CF flag contains the value of the bit shifted into it.
		The OF flag is affected only for single- bit rotates (see “Description” above);
		it is undefined for multi-bit rotates. The SF, ZF, AF, and PF flags are not affected.
	*/
	const auto temp_count = count & (Numeric::bit_size<IntT>() - 1);
	if(!count) {
		// TODO: is this 8086-specific? i.e. do the other x86s also exit without affecting flags when temp_count = 0?
		return;
	}
	if(temp_count) {
		destination =
			(destination << temp_count) |
			(destination >> (Numeric::bit_size<IntT>() - temp_count));
	}

	context.flags.template set_from<Flag::Carry>(destination & 1);
	context.flags.template set_from<Flag::Overflow>(
		((destination >> (Numeric::bit_size<IntT>() - 1)) ^ destination) & 1
	);
}

template <typename IntT, typename ContextT>
void ror(
	modify_t<IntT> destination,
	uint8_t count,
	ContextT &context
) {
	/*
		(* ROL and ROR instructions *)
		SIZE ← OperandSize
		CASE (determine count) OF
			SIZE = 8:	tempCOUNT ← COUNT MOD 8;
			SIZE = 16:	tempCOUNT ← COUNT MOD 16;
			SIZE = 32:	tempCOUNT ← COUNT MOD 32;
		ESAC;
	*/
	/*
		(* ROR instruction operation *)
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← LSB(DEST);
				DEST ← (DEST / 2) + (tempCF * 2^SIZE);
				tempCOUNT ← tempCOUNT – 1;
			OD;
		ELIHW;
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR MSB - 1 (DEST);
			ELSE OF is undefined;
		FI;
	*/
	/*
		The CF flag contains the value of the bit shifted into it.
		The OF flag is affected only for single- bit rotates (see “Description” above);
		it is undefined for multi-bit rotates. The SF, ZF, AF, and PF flags are not affected.
	*/
	const auto temp_count = count & (Numeric::bit_size<IntT>() - 1);
	if(!count) {
		// TODO: is this 8086-specific? i.e. do the other x86s also exit without affecting flags when temp_count = 0?
		return;
	}
	if(temp_count) {
		destination =
			(destination >> temp_count) |
			(destination << (Numeric::bit_size<IntT>() - temp_count));
	}

	context.flags.template set_from<Flag::Carry>(destination & Numeric::top_bit<IntT>());
	context.flags.template set_from<Flag::Overflow>(
		(destination ^ (destination << 1)) & Numeric::top_bit<IntT>()
	);
}

/*
	tempCOUNT ← (COUNT AND 1FH);
	tempDEST ← DEST;
	WHILE (tempCOUNT ≠ 0)
	DO
		IF instruction is SAL or SHL
			THEN
				CF ← MSB(DEST);
			ELSE (* instruction is SAR or SHR *)
				CF ← LSB(DEST);
		FI;
		IF instruction is SAL or SHL
			THEN
				DEST ← DEST ∗ 2;
			ELSE
				IF instruction is SAR
					THEN
						DEST ← DEST / 2 (*Signed divide, rounding toward negative infinity*);
					ELSE (* instruction is SHR *)
						DEST ← DEST / 2 ; (* Unsigned divide *);
				FI;
		FI;
		tempCOUNT ← tempCOUNT – 1;
	OD;
	(* Determine overflow for the various instructions *)
	IF COUNT = 1
		THEN
			IF instruction is SAL or SHL
				THEN
					OF ← MSB(DEST) XOR CF;
				ELSE
					IF instruction is SAR
						THEN
							OF ← 0;
						ELSE (* instruction is SHR *)
							OF ← MSB(tempDEST);
					FI;
			FI;
		ELSE
			IF COUNT = 0
				THEN
					All flags remain unchanged;
				ELSE (* COUNT neither 1 or 0 *)
					OF ← undefined;
			FI;
	FI;
*/
/*
	The CF flag contains the value of the last bit shifted out of the destination operand;
	it is undefined for SHL and SHR instructions where the count is greater than or equal to
	the size (in bits) of the destination operand. The OF flag is affected only for 1-bit shifts
	(see “Description” above); otherwise, it is undefined.

	The SF, ZF, and PF flags are set according to the result. If the count is 0, the flags are not affected.
	For a non-zero count, the AF flag is undefined.
*/
template <typename IntT, typename ContextT>
void sal(
	modify_t<IntT> destination,
	uint8_t count,
	ContextT &context
) {
	switch(count) {
		case 0:	return;
		case Numeric::bit_size<IntT>():
			context.flags.template set_from<Flag::Carry, Flag::Overflow>(destination & 1);
			destination = 0;
		break;
		default:
			if(count > Numeric::bit_size<IntT>()) {
				context.flags.template set_from<Flag::Carry, Flag::Overflow>(0);
				destination = 0;
			} else {
				const auto mask = (Numeric::top_bit<IntT>() >> (count - 1));
				context.flags.template set_from<Flag::Carry>(
					 destination & mask
				);
				context.flags.template set_from<Flag::Overflow>(
					 (destination ^ (destination << 1)) & mask
				);
				destination <<= count;
			}
		break;
	}
	context.flags.template set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

template <typename IntT, typename ContextT>
void sar(
	modify_t<IntT> destination,
	uint8_t count,
	ContextT &context
) {
	if(!count) {
		return;
	}

	const IntT sign = Numeric::top_bit<IntT>() & destination;
	if(count >= Numeric::bit_size<IntT>()) {
		destination = sign ? IntT(~0) : IntT(0);
		context.flags.template set_from<Flag::Carry>(sign);
	} else {
		const IntT mask = 1 << (count - 1);
		context.flags.template set_from<Flag::Carry>(destination & mask);
		destination = (destination >> count) | (sign ? ~(IntT(~0) >> count) : 0);
	}
	context.flags.template set_from<Flag::Overflow>(0);
	context.flags.template set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

template <typename IntT, typename ContextT>
void shr(
	modify_t<IntT> destination,
	uint8_t count,
	ContextT &context
) {
	if(!count) {
		return;
	}

	context.flags.template set_from<Flag::Overflow>(Numeric::top_bit<IntT>() & destination);
	if(count == Numeric::bit_size<IntT>()) {
		context.flags.template set_from<Flag::Carry>(Numeric::top_bit<IntT>() & destination);
		destination = 0;
	} else if(count > Numeric::bit_size<IntT>()) {
		context.flags.template set_from<Flag::Carry>(0);
		destination = 0;
	} else {
		const IntT mask = 1 << (count - 1);
		context.flags.template set_from<Flag::Carry>(destination & mask);
		destination >>= count;
	}
	context.flags.template set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

}

#endif /* ShiftRoll_hpp */
