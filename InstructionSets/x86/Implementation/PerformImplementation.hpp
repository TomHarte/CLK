//
//
//  PerformImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef PerformImplementation_h
#define PerformImplementation_h

#include "../../../Numeric/Carry.hpp"
#include "../../../Numeric/RegisterSizes.hpp"
#include "../Interrupts.hpp"
#include "../AccessType.hpp"
#include "Resolver.hpp"

#include <utility>

namespace InstructionSet::x86 {

namespace Primitive {

// The below takes a reference in order properly to handle PUSH SP,
// which should place the value of SP after the push onto the stack.
template <typename IntT, bool preauthorised, typename ContextT>
void push(IntT &value, ContextT &context) {
	context.registers.sp_ -= sizeof(IntT);
	if constexpr (preauthorised) {
		context.memory.template preauthorised_write<IntT>(Source::SS, context.registers.sp_, value);
	} else {
		context.memory.template access<IntT, AccessType::Write>(
			Source::SS,
			context.registers.sp_) = value;
	}
	context.memory.template write_back<IntT>();
}

template <typename IntT, bool preauthorised, typename ContextT>
IntT pop(ContextT &context) {
	const auto value = context.memory.template access<IntT, preauthorised ? AccessType::PreauthorisedRead : AccessType::Read>(
		Source::SS,
		context.registers.sp_);
	context.registers.sp_ += sizeof(IntT);
	return value;
}

//
// Comments below on intended functioning of each operation come from the 1997 edition of the
// Intel Architecture Software Developer’s Manual; that year all such definitions still fitted within a
// single volume, Volume 2.
//
// Order Number 243191; e.g. https://www.ardent-tool.com/CPU/docs/Intel/IA/243191-002.pdf
//

template <typename ContextT>
void aaa(CPU::RegisterPair16 &ax, ContextT &context) {	// P. 313
	/*
		IF ((AL AND 0FH) > 9) OR (AF = 1)
			THEN
				AL ← (AL + 6);
				AH ← AH + 1;
				AF ← 1;
				CF ← 1;
			ELSE
				AF ← 0;
				CF ← 0;
			FI;
		AL ← AL AND 0FH;
	*/
	/*
		The AF and CF flags are set to 1 if the adjustment results in a decimal carry;
		otherwise they are cleared to 0. The OF, SF, ZF, and PF flags are undefined.
	*/
	if((ax.halves.low & 0x0f) > 9 || context.flags.template flag<Flag::AuxiliaryCarry>()) {
		ax.halves.low += 6;
		++ax.halves.high;
		context.flags.template set_from<Flag::Carry, Flag::AuxiliaryCarry>(1);
	} else {
		context.flags.template set_from<Flag::Carry, Flag::AuxiliaryCarry>(0);
	}
	ax.halves.low &= 0x0f;
}

template <typename ContextT>
void aad(CPU::RegisterPair16 &ax, uint8_t imm, ContextT &context) {
	/*
		tempAL ← AL;
		tempAH ← AH;
		AL ← (tempAL + (tempAH * imm8)) AND FFH; (* imm8 is set to 0AH for the AAD mnemonic *)
		AH ← 0
	*/
	/*
		The SF, ZF, and PF flags are set according to the result;
		the OF, AF, and CF flags are undefined.
	*/
	ax.halves.low = ax.halves.low + (ax.halves.high * imm);
	ax.halves.high = 0;
	context.flags.template set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(ax.halves.low);
}

template <typename ContextT>
void aam(CPU::RegisterPair16 &ax, uint8_t imm, ContextT &context) {
	/*
		tempAL ← AL;
		AH ← tempAL / imm8; (* imm8 is set to 0AH for the AAD mnemonic *)
		AL ← tempAL MOD imm8;
	*/
	/*
		The SF, ZF, and PF flags are set according to the result.
		The OF, AF, and CF flags are undefined.
	*/
	/*
		If ... an immediate value of 0 is used, it will cause a #DE (divide error) exception.
	*/
	if(!imm) {
		interrupt(Interrupt::DivideError, context);
		return;
	}

	ax.halves.high = ax.halves.low / imm;
	ax.halves.low = ax.halves.low % imm;
	context.flags.template set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(ax.halves.low);
}

template <typename ContextT>
void aas(CPU::RegisterPair16 &ax, ContextT &context) {
	/*
		IF ((AL AND 0FH) > 9) OR (AF = 1)
		THEN
			AL ← AL – 6;
			AH ← AH – 1;
			AF ← 1;
			CF ← 1;
		ELSE
			CF ← 0;
			AF ← 0;
		FI;
		AL ← AL AND 0FH;
	*/
	/*
		The AF and CF flags are set to 1 if there is a decimal borrow;
		otherwise, they are cleared to 0. The OF, SF, ZF, and PF flags are undefined.
	*/
	if((ax.halves.low & 0x0f) > 9 || context.flags.template flag<Flag::AuxiliaryCarry>()) {
		ax.halves.low -= 6;
		--ax.halves.high;
		context.flags.template set_from<Flag::Carry, Flag::AuxiliaryCarry>(1);
	} else {
		context.flags.template set_from<Flag::Carry, Flag::AuxiliaryCarry>(0);
	}
	ax.halves.low &= 0x0f;
}

template <typename ContextT>
void daa(uint8_t &al, ContextT &context) {
	/*
		(as modified by https://www.felixcloutier.com/x86/daa ...)

        old_AL ← AL;
        old_CF ← CF;
        CF ← 0;

		IF (((AL AND 0FH) > 9) or AF = 1)
			THEN
				AL ← AL + 6;
				CF ← old_CF OR CarryFromLastAddition; (* CF OR carry from AL ← AL + 6 *)
				AF ← 1;
			ELSE
				AF ← 0;
		FI;
		IF ((old_AL > 99H) or old_CF = 1)
			THEN
				AL ← AL + 60H;
				CF ← 1;
			ELSE
				CF ← 0;
		FI;
	*/
	/*
		The CF and AF flags are set if the adjustment of the value results in a
		decimal carry in either digit of the result (see the “Operation” section above).
		The SF, ZF, and PF flags are set according to the result. The OF flag is undefined.
	*/
	const uint8_t old_al = al;
	const auto old_carry = context.flags.template flag<Flag::Carry>();
	context.flags.template set_from<Flag::Carry>(0);

	if((al & 0x0f) > 0x09 || context.flags.template flag<Flag::AuxiliaryCarry>()) {
		context.flags.template set_from<Flag::Carry>(old_carry | (al > 0xf9));
		al += 0x06;
		context.flags.template set_from<Flag::AuxiliaryCarry>(1);
	} else {
		context.flags.template set_from<Flag::AuxiliaryCarry>(0);
	}

	if(old_al > 0x99 || old_carry) {
		al += 0x60;
		context.flags.template set_from<Flag::Carry>(1);
	} else {
		context.flags.template set_from<Flag::Carry>(0);
	}

	context.flags.template set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(al);
}

template <typename ContextT>
void das(uint8_t &al, ContextT &context) {
	/*
		(as modified by https://www.felixcloutier.com/x86/daa ...)

        old_AL ← AL;
        old_CF ← CF;
        CF ← 0;

		IF (((AL AND 0FH) > 9) or AF = 1)
			THEN
				AL ← AL - 6;
				CF ← old_CF OR CarryFromLastAddition; (* CF OR borrow from AL ← AL - 6 *)
				AF ← 1;
			ELSE
				AF ← 0;
		FI;
		IF ((old_AL > 99H) or old_CF = 1)
			THEN
				AL ← AL - 60H;
				CF ← 1;
			ELSE
				CF ← 0;
		FI;
	*/
	/*
		The CF and AF flags are set if the adjustment of the value results in a
		decimal carry in either digit of the result (see the “Operation” section above).
		The SF, ZF, and PF flags are set according to the result. The OF flag is undefined.
	*/
	const uint8_t old_al = al;
	const auto old_carry = context.flags.template flag<Flag::Carry>();
	context.flags.template set_from<Flag::Carry>(0);

	if((al & 0x0f) > 0x09 || context.flags.template flag<Flag::AuxiliaryCarry>()) {
		context.flags.template set_from<Flag::Carry>(old_carry | (al < 0x06));
		al -= 0x06;
		context.flags.template set_from<Flag::AuxiliaryCarry>(1);
	} else {
		context.flags.template set_from<Flag::AuxiliaryCarry>(0);
	}

	if(old_al > 0x99 || old_carry) {
		al -= 0x60;
		context.flags.template set_from<Flag::Carry>(1);
	} else {
		context.flags.template set_from<Flag::Carry>(0);
	}

	context.flags.template set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(al);
}

template <bool with_carry, typename IntT, typename ContextT>
void add(IntT &destination, IntT source, ContextT &context) {
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

	if constexpr (destination_type == AccessType::Write) {
		destination = result;
	}
}

template <typename IntT, typename ContextT>
void test(IntT &destination, IntT source, ContextT &context) {
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

template <typename IntT>
void xchg(IntT &destination, IntT &source) {
	/*
		TEMP ← DEST
		DEST ← SRC
		SRC ← TEMP
	*/
	std::swap(destination, source);
}

template <typename IntT, typename ContextT>
void mul(IntT &destination_high, IntT &destination_low, IntT source, ContextT &context) {
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
void imul(IntT &destination_high, IntT &destination_low, IntT source, ContextT &context) {
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
void div(IntT &destination_high, IntT &destination_low, IntT source, ContextT &context) {
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

template <typename IntT, typename ContextT>
void idiv(IntT &destination_high, IntT &destination_low, IntT source, ContextT &context) {
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
	const auto result = dividend / sIntT(source);
	if(sIntT(result) != result) {
		interrupt(Interrupt::DivideError, context);
		return;
	}

	destination_low = IntT(result);
	destination_high = dividend % sIntT(source);
}

template <typename IntT, typename ContextT>
void inc(IntT &destination, ContextT &context) {
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
void jump(bool condition, IntT displacement, ContextT &context) {
	/*
		IF condition
			THEN
				EIP ← EIP + SignExtend(DEST);
				IF OperandSize = 16
					THEN
						EIP ← EIP AND 0000FFFFH;
				FI;
		FI;
	*/

	// TODO: proper behaviour in 32-bit.
	if(condition) {
		context.flow_controller.jump(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loop(IntT &counter, OffsetT displacement, ContextT &context) {
	--counter;
	if(counter) {
		context.flow_controller.jump(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loope(IntT &counter, OffsetT displacement, ContextT &context) {
	--counter;
	if(counter && context.flags.template flag<Flag::Zero>()) {
		context.flow_controller.jump(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loopne(IntT &counter, OffsetT displacement, ContextT &context) {
	--counter;
	if(counter && !context.flags.template flag<Flag::Zero>()) {
		context.flow_controller.jump(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename ContextT>
void dec(IntT &destination, ContextT &context) {
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
void and_(IntT &destination, IntT source, ContextT &context) {
	/*
		DEST ← DEST AND SRC;
	*/
	/*
		The OF and CF flags are cleared; the SF, ZF, and PF flags are set according to the result.
		The state of the AF flag is undefined.
	*/
	destination &= source;

	context.flags.template set_from<Flag::Overflow, Flag::Carry>(0);
	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
}

template <typename IntT, typename ContextT>
void or_(IntT &destination, IntT source, ContextT &context) {
	/*
		DEST ← DEST OR SRC;
	*/
	/*
		The OF and CF flags are cleared; the SF, ZF, and PF flags are set according to the result.
		The state of the AF flag is undefined.
	*/
	destination |= source;

	context.flags.template set_from<Flag::Overflow, Flag::Carry>(0);
	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
}

template <typename IntT, typename ContextT>
void xor_(IntT &destination, IntT source, ContextT &context) {
	/*
		DEST ← DEST XOR SRC;
	*/
	/*
		The OF and CF flags are cleared; the SF, ZF, and PF flags are set according to the result.
		The state of the AF flag is undefined.
	*/
	destination ^= source;

	context.flags.template set_from<Flag::Overflow, Flag::Carry>(0);
	context.flags.template set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
}

template <typename IntT, typename ContextT>
void neg(IntT &destination, ContextT &context) {
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

template <typename IntT>
void not_(IntT &destination) {
	/*
		DEST ← NOT DEST;
	*/
	/*
		Flags affected: none.
	*/
	destination  = ~destination;
}

template <typename IntT, typename ContextT>
void call_relative(IntT offset, ContextT &context) {
	push<uint16_t, false>(context.registers.ip(), context);
	context.flow_controller.jump(context.registers.ip() + offset);
}

template <typename IntT, typename ContextT>
void call_absolute(IntT target, ContextT &context) {
	push<uint16_t, false>(context.registers.ip(), context);
	context.flow_controller.jump(target);
}

template <typename IntT, typename ContextT>
void jump_absolute(IntT target, ContextT &context) {
	context.flow_controller.jump(target);
}

template <typename InstructionT, typename ContextT>
void call_far(InstructionT &instruction, ContextT &context) {
	// TODO: eliminate 16-bit assumption below.
	const Source source_segment = instruction.data_segment();
	context.memory.preauthorise_stack_write(sizeof(uint16_t) * 2);

	uint16_t source_address;
	const auto pointer = instruction.destination();
	switch(pointer.source()) {
		default:
		case Source::Immediate:
			push<uint16_t, true>(context.registers.cs(), context);
			push<uint16_t, true>(context.registers.ip(), context);
			context.flow_controller.jump(instruction.segment(), instruction.offset());
		return;

		case Source::Indirect:
			source_address = address<Source::Indirect, uint16_t, AccessType::Read>(instruction, pointer, context);
		break;
		case Source::IndirectNoBase:
			source_address = address<Source::IndirectNoBase, uint16_t, AccessType::Read>(instruction, pointer, context);
		break;
		case Source::DirectAddress:
			source_address = address<Source::DirectAddress, uint16_t, AccessType::Read>(instruction, pointer, context);
		break;
	}

	context.memory.preauthorise_read(source_segment, source_address, sizeof(uint16_t) * 2);
	const uint16_t offset = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);
	source_address += 2;
	const uint16_t segment = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);

	// At least on an 8086, the stack writes occur after the target address read.
	push<uint16_t, true>(context.registers.cs(), context);
	push<uint16_t, true>(context.registers.ip(), context);

	context.flow_controller.jump(segment, offset);
}

template <typename InstructionT, typename ContextT>
void jump_far(InstructionT &instruction, ContextT &context) {
	// TODO: eliminate 16-bit assumption below.
	uint16_t source_address = 0;
	const auto pointer = instruction.destination();
	switch(pointer.source()) {
		default:
		case Source::Immediate:	context.flow_controller.jump(instruction.segment(), instruction.offset());	return;

		case Source::Indirect:
			source_address = address<Source::Indirect, uint16_t, AccessType::Read>(instruction, pointer, context);
		break;
		case Source::IndirectNoBase:
			source_address = address<Source::IndirectNoBase, uint16_t, AccessType::Read>(instruction, pointer, context);
		break;
		case Source::DirectAddress:
			source_address = address<Source::DirectAddress, uint16_t, AccessType::Read>(instruction, pointer, context);
		break;
	}

	const Source source_segment = instruction.data_segment();
	context.memory.preauthorise_read(source_segment, source_address, sizeof(uint16_t) * 2);

	const uint16_t offset = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);
	source_address += 2;
	const uint16_t segment = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);
	context.flow_controller.jump(segment, offset);
}

template <typename ContextT>
void iret(ContextT &context) {
	// TODO: all modes other than 16-bit real mode.
	context.memory.preauthorise_stack_read(sizeof(uint16_t) * 3);
	const auto ip = pop<uint16_t, true>(context);
	const auto cs = pop<uint16_t, true>(context);
	context.flags.set(pop<uint16_t, true>(context));
	context.flow_controller.jump(cs, ip);
}

template <typename InstructionT, typename ContextT>
void ret_near(InstructionT instruction, ContextT &context) {
	const auto ip = pop<uint16_t, false>(context);
	context.registers.sp() += instruction.operand();
	context.flow_controller.jump(ip);
}

template <typename InstructionT, typename ContextT>
void ret_far(InstructionT instruction, ContextT &context) {
	context.memory.preauthorise_stack_read(sizeof(uint16_t) * 2);
	const auto ip = pop<uint16_t, true>(context);
	const auto cs = pop<uint16_t, true>(context);
	context.registers.sp() += instruction.operand();
	context.flow_controller.jump(cs, ip);
}

template <Source selector, typename InstructionT, typename ContextT>
void ld(
	InstructionT &instruction,
	uint16_t &destination,
	ContextT &context
) {
	const auto pointer = instruction.source();
	auto source_address = address<uint16_t, AccessType::Read>(instruction, pointer, context);
	const Source source_segment = instruction.data_segment();

	context.memory.preauthorise_read(source_segment, source_address, 4);
	destination = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);
	source_address += 2;
	switch(selector) {
		case Source::DS:	context.registers.ds() = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);	break;
		case Source::ES:	context.registers.es() = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);	break;
	}
}

template <typename IntT, typename InstructionT, typename ContextT>
void lea(
	const InstructionT &instruction,
	IntT &destination,
	ContextT &context
) {
	// TODO: address size.
	destination = IntT(address<uint16_t, AccessType::Read>(instruction, instruction.source(), context));
}

template <typename AddressT, typename InstructionT, typename ContextT>
void xlat(
	const InstructionT &instruction,
	ContextT &context
) {
	AddressT address;
	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		address = context.registers.bx() + context.registers.al();
	}

	context.registers.al() = context.memory.template access<uint8_t, AccessType::Read>(instruction.data_segment(), address);
}

template <typename IntT>
void mov(IntT &destination, IntT source) {
	destination = source;
}

template <typename ContextT>
void into(ContextT &context) {
	if(context.flags.template flag<Flag::Overflow>()) {
		interrupt(Interrupt::OnOverflow, context);
	}
}

template <typename ContextT>
void sahf(uint8_t &ah, ContextT &context) {
	/*
		EFLAGS(SF:ZF:0:AF:0:PF:1:CF) ← AH;
	*/
	context.flags.template set_from<uint8_t, Flag::Sign>(ah);
	context.flags.template set_from<Flag::Zero>(!(ah & 0x40));
	context.flags.template set_from<Flag::AuxiliaryCarry>(ah & 0x10);
	context.flags.template set_from<Flag::ParityOdd>(!(ah & 0x04));
	context.flags.template set_from<Flag::Carry>(ah & 0x01);
}

template <typename ContextT>
void lahf(uint8_t &ah, ContextT &context) {
	/*
		AH ← EFLAGS(SF:ZF:0:AF:0:PF:1:CF);
	*/
	ah =
		(context.flags.template flag<Flag::Sign>() ? 0x80 : 0x00)	|
		(context.flags.template flag<Flag::Zero>() ? 0x40 : 0x00)	|
		(context.flags.template flag<Flag::AuxiliaryCarry>() ? 0x10 : 0x00)	|
		(context.flags.template flag<Flag::ParityOdd>() ? 0x00 : 0x04)	|
		0x02 |
		(context.flags.template flag<Flag::Carry>() ? 0x01 : 0x00);
}

template <typename IntT>
void cbw(IntT &ax) {
	constexpr IntT test_bit = 1 << (sizeof(IntT) * 4 - 1);
	constexpr IntT low_half = (1 << (sizeof(IntT) * 4)) - 1;

	if(ax & test_bit) {
		ax |= ~low_half;
	} else {
		ax &= low_half;
	}
}

template <typename IntT>
void cwd(IntT &dx, IntT ax) {
	dx = ax & Numeric::top_bit<IntT>() ? IntT(~0) : IntT(0);
}

// TODO: changes to the interrupt flag do quite a lot more in protected mode.
template <typename ContextT>
void clc(ContextT &context) {	context.flags.template set_from<Flag::Carry>(0);							}
template <typename ContextT>
void cld(ContextT &context) {	context.flags.template set_from<Flag::Direction>(0);						}
template <typename ContextT>
void cli(ContextT &context) {	context.flags.template set_from<Flag::Interrupt>(0);						}
template <typename ContextT>
void stc(ContextT &context) {	context.flags.template set_from<Flag::Carry>(1);							}
template <typename ContextT>
void std(ContextT &context) {	context.flags.template set_from<Flag::Direction>(1);						}
template <typename ContextT>
void sti(ContextT &context) {	context.flags.template set_from<Flag::Interrupt>(1);						}
template <typename ContextT>
void cmc(ContextT &context) {
	context.flags.template set_from<Flag::Carry>(!context.flags.template flag<Flag::Carry>());
}

template <typename ContextT>
void salc(uint8_t &al, ContextT &context) {
	al = context.flags.template flag<Flag::Carry>() ? 0xff : 0x00;
}

template <typename IntT, typename ContextT>
void setmo(IntT &destination, ContextT &context) {
	destination = ~0;
	context.flags.template set_from<Flag::Carry, Flag::AuxiliaryCarry, Flag::Overflow>(0);
	context.flags.template set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

template <typename IntT, typename ContextT>
void rcl(IntT &destination, uint8_t count, ContextT &context) {
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
void rcr(IntT &destination, uint8_t count, ContextT &context) {
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
void rol(IntT &destination, uint8_t count, ContextT &context) {
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
void ror(IntT &destination, uint8_t count, ContextT &context) {
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
void sal(IntT &destination, uint8_t count, ContextT &context) {
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
void sar(IntT &destination, uint8_t count, ContextT &context) {
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
void shr(IntT &destination, uint8_t count, ContextT &context) {
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

template <typename ContextT>
void popf(ContextT &context) {
	context.flags.set(pop<uint16_t, false>(context));
}

template <typename ContextT>
void pushf(ContextT &context) {
	uint16_t value = context.flags.get();
	push<uint16_t, false>(value, context);
}

template <typename AddressT, Repetition repetition>
bool repetition_over(const AddressT &eCX) {
	return repetition != Repetition::None && !eCX;
}

template <typename AddressT, Repetition repetition, typename ContextT>
void repeat(AddressT &eCX, ContextT &context) {
	if(
		repetition == Repetition::None ||		// No repetition => stop.
		!(--eCX)								// [e]cx is zero after being decremented => stop.
	) {
		return;
	}
	if constexpr (repetition != Repetition::Rep) {
		// If this is RepE or RepNE, also test the zero flag.
		if((repetition == Repetition::RepNE) == context.flags.template flag<Flag::Zero>()) {
			return;
		}
	}
	context.flow_controller.repeat_last();
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void cmps(const InstructionT &instruction, AddressT &eCX, AddressT &eSI, AddressT &eDI, ContextT &context) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	IntT lhs = context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI);
	const IntT rhs = context.memory.template access<IntT, AccessType::Read>(Source::ES, eDI);
	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	Primitive::sub<false, false>(lhs, rhs, context);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename ContextT>
void scas(AddressT &eCX, AddressT &eDI, IntT &eAX, ContextT &context) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	const IntT rhs = context.memory.template access<IntT, AccessType::Read>(Source::ES, eDI);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	Primitive::sub<false, false>(eAX, rhs, context);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void lods(const InstructionT &instruction, AddressT &eCX, AddressT &eSI, IntT &eAX, ContextT &context) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	eAX = context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI);
	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void movs(const InstructionT &instruction, AddressT &eCX, AddressT &eSI, AddressT &eDI, ContextT &context) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	context.memory.template access<IntT, AccessType::Write>(Source::ES, eDI) =
		context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI);

	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename ContextT>
void stos(AddressT &eCX, AddressT &eDI, IntT &eAX, ContextT &context) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	context.memory.template access<IntT, AccessType::Write>(Source::ES, eDI) = eAX;
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename InstructionT, typename ContextT>
void outs(const InstructionT &instruction, AddressT &eCX, uint16_t port, AddressT &eSI, ContextT &context) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	context.io.template out<IntT>(
		port,
		context.memory.template access<IntT, AccessType::Read>(instruction.data_segment(), eSI)
	);
	eSI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename AddressT, Repetition repetition, typename ContextT>
void ins(AddressT &eCX, uint16_t port, AddressT &eDI, ContextT &context) {
	if(repetition_over<AddressT, repetition>(eCX)) {
		return;
	}

	context.memory.template access<IntT, AccessType::Write>(Source::ES, eDI) = context.io.template in<IntT>(port);
	eDI += context.flags.template direction<AddressT>() * sizeof(IntT);

	repeat<AddressT, repetition>(eCX, context);
}

template <typename IntT, typename ContextT>
void out(uint16_t port, IntT value, ContextT &context) {
	context.io.template out<IntT>(port, value);
}

template <typename IntT, typename ContextT>
void in(uint16_t port, IntT &value, ContextT &context) {
	value = context.io.template in<IntT>(port);
}

}

template <
	DataSize data_size,
	AddressSize address_size,
	typename InstructionT,
	typename ContextT
> void perform(
	const InstructionT &instruction,
	ContextT &context
) {
	using IntT = typename DataSizeType<data_size>::type;
	using AddressT = typename AddressSizeType<address_size>::type;

	// Establish source() and destination() shorthands to fetch data if necessary.
	//
	// C++17, which this project targets at the time of writing, does not provide templatised lambdas.
	// So the following division is in part a necessity.
	//
	// (though GCC offers C++20 syntax as an extension, and Clang seems to follow along, so maybe I'm overthinking)
	IntT immediate;
	const auto source_r = [&]() -> IntT {
		return resolve<IntT, AccessType::Read>(
			instruction,
			instruction.source().source(),
			instruction.source(),
			context,
			nullptr,
			&immediate);
	};
	const auto source_rmw = [&]() -> IntT& {
		return resolve<IntT, AccessType::ReadModifyWrite>(
			instruction,
			instruction.source().source(),
			instruction.source(),
			context,
			nullptr,
			&immediate);
	};
	const auto destination_r = [&]() -> IntT {
		return resolve<IntT, AccessType::Read>(
			instruction,
			instruction.destination().source(),
			instruction.destination(),
			context,
			nullptr,
			&immediate);
	};
	const auto destination_w = [&]() -> IntT& {
		return resolve<IntT, AccessType::Write>(
			instruction,
			instruction.destination().source(),
			instruction.destination(),
			context,
			nullptr,
			&immediate);
	};
	const auto destination_rmw = [&]() -> IntT& {
		return resolve<IntT, AccessType::ReadModifyWrite>(
			instruction,
			instruction.destination().source(),
			instruction.destination(),
			context,
			nullptr,
			&immediate);
	};

	// Performs a displacement jump only if @c condition is true.
	const auto jcc = [&](bool condition) {
		Primitive::jump(
			condition,
			instruction.displacement(),
			context);
	};

	const auto shift_count = [&]() -> uint8_t {
		static constexpr uint8_t mask = (ContextT::model != Model::i8086) ? 0x1f : 0xff;
		switch(instruction.source().source()) {
			case Source::None:		return 1;
			case Source::Immediate:	return uint8_t(instruction.operand()) & mask;
			default:				return context.registers.cl() & mask;
		}
	};

	// Some instructions use a pair of registers as an extended accumulator — DX:AX or EDX:EAX.
	// The two following return the high and low parts of that pair; they also work in Byte mode to return AH:AL,
	// i.e. AX split into high and low parts.
	const auto pair_high = [&]() -> IntT& {
		if constexpr (data_size == DataSize::Byte) 			return context.registers.ah();
		else if constexpr (data_size == DataSize::Word)		return context.registers.dx();
		else if constexpr (data_size == DataSize::DWord)	return context.registers.edx();
	};
	const auto pair_low = [&]() -> IntT& {
		if constexpr (data_size == DataSize::Byte) 			return context.registers.al();
		else if constexpr (data_size == DataSize::Word)		return context.registers.ax();
		else if constexpr (data_size == DataSize::DWord)	return context.registers.eax();
	};

	// For the string operations, evaluate to either SI and DI or ESI and EDI, depending on the address size.
	const auto eSI = [&]() -> AddressT& {
		if constexpr (std::is_same_v<AddressT, uint16_t>) {
			return context.registers.si();
		} else {
			return context.registers.esi();
		}
	};
	const auto eDI = [&]() -> AddressT& {
		if constexpr (std::is_same_v<AddressT, uint16_t>) {
			return context.registers.di();
		} else {
			return context.registers.edi();
		}
	};

	// For counts, provide either eCX or CX depending on address size.
	const auto eCX = [&]() -> AddressT& {
		if constexpr (std::is_same_v<AddressT, uint16_t>) {
			return context.registers.cx();
		} else {
			return context.registers.ecx();
		}
	};

	// Gets the port for an IN or OUT; these are always 16-bit.
	const auto port = [&](Source source) -> uint16_t {
		switch(source) {
			case Source::DirectAddress:	return instruction.offset();
			default:					return context.registers.dx();
		}
	};

	// Guide to the below:
	//
	//	* use hard-coded register names where appropriate;
	//	* return directly if there is definitely no possible write back to RAM;
	//	* otherwise use the source() and destination() lambdas, and break in order to allow a writeback if necessary.
	switch(instruction.operation()) {
		default:
			assert(false);

		case Operation::AAA:	Primitive::aaa(context.registers.axp(), context);							return;
		case Operation::AAD:	Primitive::aad(context.registers.axp(), instruction.operand(), context);	return;
		case Operation::AAM:	Primitive::aam(context.registers.axp(), instruction.operand(), context);	return;
		case Operation::AAS:	Primitive::aas(context.registers.axp(), context);							return;
		case Operation::DAA:	Primitive::daa(context.registers.al(), context);							return;
		case Operation::DAS:	Primitive::das(context.registers.al(), context);							return;

		case Operation::CBW:	Primitive::cbw(pair_low());					return;
		case Operation::CWD:	Primitive::cwd(pair_high(), pair_low());	return;

		case Operation::ESC:
		case Operation::NOP:	return;

		case Operation::HLT:	context.flow_controller.halt();		return;
		case Operation::WAIT:	context.flow_controller.wait();		return;

		case Operation::ADC:	Primitive::add<true>(destination_rmw(), source_r(), context);			break;
		case Operation::ADD:	Primitive::add<false>(destination_rmw(), source_r(), context);			break;
		case Operation::SBB:	Primitive::sub<true, true>(destination_rmw(), source_r(), context);		break;
		case Operation::SUB:	Primitive::sub<false, true>(destination_rmw(), source_r(), context);	break;
		case Operation::CMP:	Primitive::sub<false, false>(destination_r(), source_r(), context);		return;
		case Operation::TEST:	Primitive::test(destination_r(), source_r(), context);					return;

		case Operation::MUL:	Primitive::mul(pair_high(), pair_low(), source_r(), context);			return;
		case Operation::IMUL_1:	Primitive::imul(pair_high(), pair_low(), source_r(), context);			return;
		case Operation::DIV:	Primitive::div(pair_high(), pair_low(), source_r(), context);			return;
		case Operation::IDIV:	Primitive::idiv(pair_high(), pair_low(), source_r(), context);			return;

		case Operation::INC:	Primitive::inc(destination_rmw(), context);		break;
		case Operation::DEC:	Primitive::dec(destination_rmw(), context);		break;

		case Operation::AND:	Primitive::and_(destination_rmw(), source_r(), context);	break;
		case Operation::OR:		Primitive::or_(destination_rmw(), source_r(), context);		break;
		case Operation::XOR:	Primitive::xor_(destination_rmw(), source_r(), context);	break;
		case Operation::NEG:	Primitive::neg(source_rmw(), context);						break;	// TODO: should be a destination.
		case Operation::NOT:	Primitive::not_(source_rmw());								break;	// TODO: should be a destination.

		case Operation::CALLrel:	Primitive::call_relative(instruction.displacement(), context);		return;
		case Operation::CALLabs:	Primitive::call_absolute(destination_r(), context);					return;
		case Operation::CALLfar:	Primitive::call_far(instruction, context);							return;

		case Operation::JMPrel:	jcc(true);												return;
		case Operation::JMPabs:	Primitive::jump_absolute(destination_r(), context);		return;
		case Operation::JMPfar:	Primitive::jump_far(instruction, context);				return;

		case Operation::JCXZ:	jcc(!eCX());												return;
		case Operation::LOOP:	Primitive::loop(eCX(), instruction.offset(), context);		return;
		case Operation::LOOPE:	Primitive::loope(eCX(), instruction.offset(), context);		return;
		case Operation::LOOPNE:	Primitive::loopne(eCX(), instruction.offset(), context);	return;

		case Operation::IRET:		Primitive::iret(context);					return;
		case Operation::RETnear:	Primitive::ret_near(instruction, context);	return;
		case Operation::RETfar:		Primitive::ret_far(instruction, context);	return;

		case Operation::INT:	interrupt(instruction.operand(), context);		return;
		case Operation::INTO:	Primitive::into(context);						return;

		case Operation::SAHF:	Primitive::sahf(context.registers.ah(), context);		return;
		case Operation::LAHF:	Primitive::lahf(context.registers.ah(), context);		return;

		case Operation::LDS:	if constexpr (data_size == DataSize::Word) Primitive::ld<Source::DS>(instruction, destination_w(), context);	return;
		case Operation::LES:	if constexpr (data_size == DataSize::Word) Primitive::ld<Source::ES>(instruction, destination_w(), context);	return;

		case Operation::LEA:	Primitive::lea(instruction, destination_w(), context);	return;
		case Operation::MOV:	Primitive::mov(destination_w(), source_r());			break;

		case Operation::JO:		jcc(context.flags.template condition<Condition::Overflow>());		return;
		case Operation::JNO:	jcc(!context.flags.template condition<Condition::Overflow>());		return;
		case Operation::JB:		jcc(context.flags.template condition<Condition::Below>());			return;
		case Operation::JNB:	jcc(!context.flags.template condition<Condition::Below>());		return;
		case Operation::JZ:		jcc(context.flags.template condition<Condition::Zero>());			return;
		case Operation::JNZ:	jcc(!context.flags.template condition<Condition::Zero>());			return;
		case Operation::JBE:	jcc(context.flags.template condition<Condition::BelowOrEqual>());	return;
		case Operation::JNBE:	jcc(!context.flags.template condition<Condition::BelowOrEqual>());	return;
		case Operation::JS:		jcc(context.flags.template condition<Condition::Sign>());			return;
		case Operation::JNS:	jcc(!context.flags.template condition<Condition::Sign>());			return;
		case Operation::JP:		jcc(!context.flags.template condition<Condition::ParityOdd>());	return;
		case Operation::JNP:	jcc(context.flags.template condition<Condition::ParityOdd>());		return;
		case Operation::JL:		jcc(context.flags.template condition<Condition::Less>());			return;
		case Operation::JNL:	jcc(!context.flags.template condition<Condition::Less>());			return;
		case Operation::JLE:	jcc(context.flags.template condition<Condition::LessOrEqual>());	return;
		case Operation::JNLE:	jcc(!context.flags.template condition<Condition::LessOrEqual>());	return;

		case Operation::RCL:	Primitive::rcl(destination_rmw(), shift_count(), context);	break;
		case Operation::RCR:	Primitive::rcr(destination_rmw(), shift_count(), context);	break;
		case Operation::ROL:	Primitive::rol(destination_rmw(), shift_count(), context);	break;
		case Operation::ROR:	Primitive::ror(destination_rmw(), shift_count(), context);	break;
		case Operation::SAL:	Primitive::sal(destination_rmw(), shift_count(), context);	break;
		case Operation::SAR:	Primitive::sar(destination_rmw(), shift_count(), context);	break;
		case Operation::SHR:	Primitive::shr(destination_rmw(), shift_count(), context);	break;

		case Operation::CLC:	Primitive::clc(context);				return;
		case Operation::CLD:	Primitive::cld(context);				return;
		case Operation::CLI:	Primitive::cli(context);				return;
		case Operation::STC:	Primitive::stc(context);				return;
		case Operation::STD:	Primitive::std(context);				return;
		case Operation::STI:	Primitive::sti(context);				return;
		case Operation::CMC:	Primitive::cmc(context);				return;

		case Operation::XCHG:	Primitive::xchg(destination_rmw(), source_rmw());	break;

		case Operation::SALC:	Primitive::salc(context.registers.al(), context);	return;
		case Operation::SETMO:
			if constexpr (ContextT::model == Model::i8086) {
				Primitive::setmo(destination_w(), context);
				break;
			} else {
				// TODO.
			}
		return;
		case Operation::SETMOC:
			if constexpr (ContextT::model == Model::i8086) {
				// Test CL out here to avoid taking a reference to memory if
				// no write is going to occur.
				if(context.registers.cl()) {
					Primitive::setmo(destination_w(), context);
				}
				break;
			} else {
				// TODO.
			}
		return;

		case Operation::OUT: Primitive::out(port(instruction.destination().source()), pair_low(), context);	return;
		case Operation::IN:	 Primitive::in(port(instruction.source().source()), pair_low(), context);		return;

		case Operation::XLAT:	Primitive::xlat<AddressT>(instruction, context);		return;

		case Operation::POP:	destination_w() = Primitive::pop<IntT, false>(context);	break;
		case Operation::PUSH:	Primitive::push<IntT, false>(source_r(), context);		break;
		case Operation::POPF:	Primitive::popf(context);								break;
		case Operation::PUSHF:	Primitive::pushf(context);								break;

		case Operation::CMPS:
			Primitive::cmps<IntT, AddressT, Repetition::None>(instruction, eCX(), eSI(), eDI(), context);
		break;
		case Operation::CMPS_REPE:
			Primitive::cmps<IntT, AddressT, Repetition::RepE>(instruction, eCX(), eSI(), eDI(), context);
		break;
		case Operation::CMPS_REPNE:
			Primitive::cmps<IntT, AddressT, Repetition::RepNE>(instruction, eCX(), eSI(), eDI(), context);
		break;

		case Operation::SCAS:
			Primitive::scas<IntT, AddressT, Repetition::None>(eCX(), eDI(), pair_low(), context);
		break;
		case Operation::SCAS_REPE:
			Primitive::scas<IntT, AddressT, Repetition::RepE>(eCX(), eDI(), pair_low(), context);
		break;
		case Operation::SCAS_REPNE:
			Primitive::scas<IntT, AddressT, Repetition::RepNE>(eCX(), eDI(), pair_low(), context);
		break;

		case Operation::LODS:
			Primitive::lods<IntT, AddressT, Repetition::None>(instruction, eCX(), eSI(), pair_low(), context);
		break;
		case Operation::LODS_REP:
			Primitive::lods<IntT, AddressT, Repetition::Rep>(instruction, eCX(), eSI(), pair_low(), context);
		break;

		case Operation::MOVS:
			Primitive::movs<IntT, AddressT, Repetition::None>(instruction, eCX(), eSI(), eDI(), context);
		break;
		case Operation::MOVS_REP:
			Primitive::movs<IntT, AddressT, Repetition::Rep>(instruction, eCX(), eSI(), eDI(), context);
		break;

		case Operation::STOS:
			Primitive::stos<IntT, AddressT, Repetition::None>(eCX(), eDI(), pair_low(), context);
		break;
		case Operation::STOS_REP:
			Primitive::stos<IntT, AddressT, Repetition::Rep>(eCX(), eDI(), pair_low(), context);
		break;

		case Operation::OUTS:
			Primitive::outs<IntT, AddressT, Repetition::None>(instruction, eCX(), context.registers.dx(), eSI(), context);
		break;
		case Operation::OUTS_REP:
			Primitive::outs<IntT, AddressT, Repetition::Rep>(instruction, eCX(), context.registers.dx(), eSI(), context);
		break;

		case Operation::INS:
			Primitive::ins<IntT, AddressT, Repetition::None>(eCX(), context.registers.dx(), eDI(), context);
		break;
		case Operation::INS_REP:
			Primitive::ins<IntT, AddressT, Repetition::Rep>(eCX(), context.registers.dx(), eDI(), context);
		break;
	}

	// Write to memory if required to complete this operation.
	//
	// TODO: can I eliminate this with some RAII magic?
	context.memory.template write_back<IntT>();
}

template <
	typename InstructionT,
	typename ContextT
> void perform(
	const InstructionT &instruction,
	ContextT &context
) {
	auto size = [](DataSize operation_size, AddressSize address_size) constexpr -> int {
		return int(operation_size) + (int(address_size) << 2);
	};

	// Dispatch to a function specialised on data and address size.
	switch(size(instruction.operation_size(), instruction.address_size())) {
		// 16-bit combinations.
		case size(DataSize::Byte, AddressSize::b16):
			perform<DataSize::Byte, AddressSize::b16>(instruction, context);
		return;
		case size(DataSize::Word, AddressSize::b16):
			perform<DataSize::Word, AddressSize::b16>(instruction, context);
		return;

		// 32-bit combinations.
		//
		// The if constexprs below ensure that `perform` isn't compiled for incompatible data or address size and
		// model combinations. So if a caller nominates a 16-bit model it can supply registers and memory objects
		// that don't implement 32-bit registers or accesses.
		case size(DataSize::Byte, AddressSize::b32):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::Byte, AddressSize::b32>(instruction, context);
				return;
			}
		break;
		case size(DataSize::Word, AddressSize::b32):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::Word, AddressSize::b32>(instruction, context);
				return;
			}
		break;
		case size(DataSize::DWord, AddressSize::b16):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::DWord, AddressSize::b16>(instruction, context);
				return;
			}
		break;
		case size(DataSize::DWord, AddressSize::b32):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::DWord, AddressSize::b32>(instruction, context);
				return;
			}
		break;

		default: break;
	}

	// This is reachable only if the data and address size combination in use isn't available
	// on the processor model nominated.
	assert(false);
}

template <
	typename ContextT
> void interrupt(
	int index,
	ContextT &context
) {
	const uint32_t address = static_cast<uint32_t>(index) << 2;
	context.memory.preauthorise_read(address, sizeof(uint16_t) * 2);
	context.memory.preauthorise_stack_write(sizeof(uint16_t) * 3);

	const uint16_t ip = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(address);
	const uint16_t cs = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(address + 2);

	auto flags = context.flags.get();
	Primitive::push<uint16_t, true>(flags, context);
	context.flags.template set_from<Flag::Interrupt, Flag::Trap>(0);

	// Push CS and IP.
	Primitive::push<uint16_t, true>(context.registers.cs(), context);
	Primitive::push<uint16_t, true>(context.registers.ip(), context);

	// Set new destination.
	context.flow_controller.jump(cs, ip);
}

}

#endif /* PerformImplementation_h */
