//
//  Arithmetic.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Arithmetic_hpp
#define Arithmetic_hpp

#include "../AccessType.hpp"
#include "../Interrupts.hpp"
#include "../Perform.hpp"

#include "../../../Numeric/Carry.hpp"

namespace InstructionSet::x86::Primitive {

template <bool with_carry, typename IntT, typename ContextT>
void add(
	modify_t<IntT> destination,
	read_t<IntT> source,
	ContextT &context
) {
	/*
		DEST ← DEST + SRC [+ CF];
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination + source + (with_carry ? context.flags.template carry_bit<IntT>() : 0);

	context.flags.template set_from<Flag::Carry>(
		Numeric::carried_out<true, Numeric::bit_size<IntT>() - 1>(destination, source, result));
	context.flags.template set_from<Flag::AuxiliaryCarry>(
		Numeric::carried_in<4>(destination, source, result));
	context.flags.template set_from<Flag::Overflow>(
		Numeric::overflow<true, IntT>(destination, source, result));

	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(result);

	destination = result;
}

template <bool with_borrow, AccessType destination_type, typename IntT, typename ContextT>
void sub(
	access_t<IntT, destination_type> destination,
	read_t<IntT> source,
	ContextT &context
) {
	/*
		DEST ← DEST - (SRC [+ CF]);
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination - source - (with_borrow ? context.flags.template carry_bit<IntT>() : 0);

	context.flags.template set_from<Flag::Carry>(
		Numeric::carried_out<false, Numeric::bit_size<IntT>() - 1>(destination, source, result));
	context.flags.template set_from<Flag::AuxiliaryCarry>(
		Numeric::carried_in<4>(destination, source, result));
	context.flags.template set_from<Flag::Overflow>(
		Numeric::overflow<false, IntT>(destination, source, result));

	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(result);

	if constexpr (is_writeable(destination_type)) {
		destination = result;
	}
}

template <typename IntT, typename ContextT>
void test(
	read_t<IntT> destination,
	read_t<IntT> source,
	ContextT &context
) {
	/*
		TEMP ← SRC1 AND SRC2;
		SF ← MSB(TEMP);
		IF TEMP = 0
			THEN ZF ← 0;
			ELSE ZF ← 1;
		FI:
		PF ← BitwiseXNOR(TEMP[0:7]);
		CF ← 0;
		OF ← 0;
	*/
	/*
		The OF and CF flags are cleared to 0.
		The SF, ZF, and PF flags are set according to the result (see the “Operation” section above).
		The state of the AF flag is undefined.
	*/
	const IntT result = destination & source;

	context.flags.template set_from<Flag::Carry, Flag::Overflow>(0);
	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(result);
}

template <typename IntT, typename ContextT>
void mul(
	modify_t<IntT> destination_high,
	modify_t<IntT> destination_low,
	read_t<IntT> source,
	ContextT &context
) {
	/*
		IF byte operation
			THEN
				AX ← AL * SRC
			ELSE (* word or doubleword operation *)
				IF OperandSize = 16 THEN
					DX:AX ← AX * SRC
				ELSE (* OperandSize = 32 *)
					EDX:EAX ← EAX * SRC
		FI;
	*/
	/*
		The OF and CF flags are cleared to 0 if the upper half of the result is 0;
		otherwise, they are set to 1. The SF, ZF, AF, and PF flags are undefined.
	*/
	destination_high = (destination_low * source) >> (8 * sizeof(IntT));
	destination_low *= source;
	context.flags.template set_from<Flag::Overflow, Flag::Carry>(destination_high);
}

template <typename IntT, typename ContextT>
void imul(
	modify_t<IntT> destination_high,
	modify_t<IntT> destination_low,
	read_t<IntT> source,
	ContextT &context
) {
	/*
		(as modified by https://www.felixcloutier.com/x86/daa ...)

		IF (OperandSize = 8)
			THEN
				AX ← AL ∗ SRC (* signed multiplication *)
				IF (AX = SignExtend(AL))
					THEN CF = 0; OF = 0;
					ELSE CF = 1; OF = 1;
				FI;
			ELSE IF OperandSize = 16
				THEN
					DX:AX ← AX ∗ SRC (* signed multiplication *)
					IF (DX:AX = SignExtend(AX))
						THEN CF = 0; OF = 0;
						ELSE CF = 1; OF = 1;
					FI;
				ELSE (* OperandSize = 32 *)
					EDX:EAX ← EAX ∗ SRC (* signed multiplication *)
					IF (EDX:EAX = SignExtend(EAX))
						THEN CF = 0; OF = 0;
						ELSE CF = 1; OF = 1;
					FI;
		FI;
	*/
	using sIntT = typename std::make_signed<IntT>::type;
	destination_high = (sIntT(destination_low) * sIntT(source)) >> (8 * sizeof(IntT));
	destination_low = IntT(sIntT(destination_low) * sIntT(source));

	const auto sign_extension = (destination_low & Numeric::top_bit<IntT>()) ? IntT(~0) : 0;
	context.flags.template set_from<Flag::Overflow, Flag::Carry>(destination_high != sign_extension);
}

template <typename IntT, typename ContextT>
void div(
	modify_t<IntT> destination_high,
	modify_t<IntT> destination_low,
	read_t<IntT> source,
	ContextT &context
) {
	/*
		IF SRC = 0
			THEN #DE; (* divide error *)
		FI;
		IF OperandSize = 8 (* word/byte operation *)
			THEN
				temp ← AX / SRC;
				IF temp > FFH
					THEN #DE; (* divide error *) ;
					ELSE
						AL ← temp;
						AH ← AX MOD SRC;
				FI;
		ELSE
			IF OperandSize = 16 (* doubleword/word operation *)
				THEN
					temp ← DX:AX / SRC;
					IF temp > FFFFH
						THEN #DE; (* divide error *) ;
						ELSE
							AX ← temp;
							DX ← DX:AX MOD SRC;
					FI;
				ELSE (* quadword/doubleword operation *)
					temp ← EDX:EAX / SRC;
					IF temp > FFFFFFFFH
					THEN #DE; (* divide error *) ;
					ELSE
						EAX ← temp;
						EDX ← EDX:EAX MOD SRC;
					FI;
			FI;
		FI;
	*/
	/*
		The CF, OF, SF, ZF, AF, and PF flags are undefined.
	*/
	if(!source) {
		interrupt(Interrupt::DivideError, context);
		return;
	}

	// TEMPORARY HACK. Will not work with DWords.
	const uint32_t dividend = (destination_high << (8 * sizeof(IntT))) + destination_low;
	const auto result = dividend / source;
	if(IntT(result) != result) {
		interrupt(Interrupt::DivideError, context);
		return;
	}

	destination_low = IntT(result);
	destination_high = dividend % source;
}

template <bool invert, typename IntT, typename ContextT>
void idiv(
	modify_t<IntT> destination_high,
	modify_t<IntT> destination_low,
	read_t<IntT> source,
	ContextT &context
) {
	/*
		IF SRC = 0
			THEN #DE; (* divide error *)
		FI;
		IF OperandSize = 8 (* word/byte operation *)
			THEN
				temp ← AX / SRC; (* signed division *)
				IF (temp > 7FH) OR (temp < 80H) (* if a positive result is greater than 7FH or a negative result is less than 80H *)
					THEN #DE; (* divide error *) ;
					ELSE
						AL ← temp;
						AH ← AX MOD SRC;
				FI;
		ELSE
			IF OperandSize = 16 (* doubleword/word operation *)
				THEN
					temp ← DX:AX / SRC; (* signed division *)
					IF (temp > 7FFFH) OR (temp < 8000H) (* if a positive result is greater than 7FFFH or a negative result is less than 8000H *)
						THEN #DE; (* divide error *) ;
						ELSE
							AX ← temp;
							DX ← DX:AX MOD SRC;
					FI;
				ELSE (* quadword/doubleword operation *)
					temp ← EDX:EAX / SRC; (* signed division *)
					IF (temp > 7FFFFFFFH) OR (temp < 80000000H) (* if a positive result is greater than 7FFFFFFFH or a negative result is less than 80000000H *)
						THEN #DE; (* divide error *) ;
						ELSE
							EAX ← temp;
							EDX ← EDX:EAX MOD SRC;
						FI;
			FI;
		FI;
	*/
	/*
		The CF, OF, SF, ZF, AF, and PF flags are undefined.
	*/
	if(!source) {
		interrupt(Interrupt::DivideError, context);
		return;
	}

	// TEMPORARY HACK. Will not work with DWords.
	using sIntT = typename std::make_signed<IntT>::type;
	const int32_t dividend = (sIntT(destination_high) << (8 * sizeof(IntT))) + destination_low;
	auto result = dividend / sIntT(source);

	// An 8086 quirk: rep IDIV performs an IDIV that switches the sign on its result,
	// due to reuse of an internal flag.
	if constexpr (invert) {
		result = -result;
	}

	if(sIntT(result) != result) {
		interrupt(Interrupt::DivideError, context);
		return;
	}

	destination_low = IntT(result);
	destination_high = dividend % sIntT(source);
}

template <typename IntT, typename ContextT>
void inc(
	modify_t<IntT> destination,
	ContextT &context
) {
	/*
		DEST ← DEST + 1;
	*/
	/*
		The CF flag is not affected.
		The OF, SF, ZF, AF, and PF flags are set according to the result.
	*/
	++destination;

	context.flags.template set_from<Flag::Overflow>(destination == Numeric::top_bit<IntT>());
	context.flags.template set_from<Flag::AuxiliaryCarry>(((destination - 1) ^ destination) & 0x10);
	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
}

template <typename IntT, typename ContextT>
void dec(
	modify_t<IntT> destination,
	ContextT &context
) {
	/*
		DEST ← DEST - 1;
	*/
	/*
		The CF flag is not affected.
		The OF, SF, ZF, AF, and PF flags are set according to the result.
	*/
	context.flags.template set_from<Flag::Overflow>(destination == Numeric::top_bit<IntT>());

	--destination;

	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
	context.flags.template set_from<Flag::AuxiliaryCarry>(((destination + 1) ^ destination) & 0x10);
}

template <typename IntT, typename ContextT>
void neg(
	modify_t<IntT> destination,
	ContextT &context
) {
	/*
		IF DEST = 0
			THEN CF ← 0
			ELSE CF ← 1;
		FI;
		DEST ← –(DEST)
	*/
	/*
		The CF flag cleared to 0 if the source operand is 0; otherwise it is set to 1.
		The OF, SF, ZF, AF, and PF flags are set according to the result.
	*/
	context.flags.template set_from<Flag::AuxiliaryCarry>(Numeric::carried_in<4>(IntT(0), destination, IntT(-destination)));

	destination = -destination;

	context.flags.template set_from<Flag::Carry>(destination);
	context.flags.template set_from<Flag::Overflow>(destination == Numeric::top_bit<IntT>());
	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
}

}

#endif /* Arithmetic_hpp */
