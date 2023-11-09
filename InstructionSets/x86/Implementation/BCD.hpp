//
//  BCD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef BCD_h
#define BCD_h

#include "../AccessType.hpp"

#include "../../../Numeric/RegisterSizes.hpp"

namespace InstructionSet::x86::Primitive {

template <typename ContextT>
void aaa(
	CPU::RegisterPair16 &ax,
	ContextT &context
) {	// P. 313
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
void aad(
	CPU::RegisterPair16 &ax,
	uint8_t imm,
	ContextT &context
) {
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
void aam(
	CPU::RegisterPair16 &ax,
	uint8_t imm,
	ContextT &context
) {
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
void aas(
	CPU::RegisterPair16 &ax,
	ContextT &context
) {
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
void daa(
	uint8_t &al,
	ContextT &context
) {
	bool top_exceeded_threshold;
	if constexpr (ContextT::model == Model::i8086) {
		top_exceeded_threshold = al > (context.flags.template flag<Flag::AuxiliaryCarry>() ? 0x9f : 0x99);
	} else {
		top_exceeded_threshold = al > 0x99;
	}

	if((al & 0x0f) > 0x09 || context.flags.template flag<Flag::AuxiliaryCarry>()) {
		al += 0x06;
		context.flags.template set_from<Flag::AuxiliaryCarry>(1);
	}

	if(top_exceeded_threshold || context.flags.template flag<Flag::Carry>()) {
		al += 0x60;
		context.flags.template set_from<Flag::Carry>(1);
	}

	context.flags.template set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(al);
}

template <typename ContextT>
void das(
	uint8_t &al,
	ContextT &context
) {
	const uint8_t old_al = al;

	if((al & 0x0f) > 0x09 || context.flags.template flag<Flag::AuxiliaryCarry>()) {
		al -= 0x06;
		context.flags.template set_from<Flag::AuxiliaryCarry>(1);
	}

	if(old_al > 0x99 || context.flags.template flag<Flag::Carry>()) {
		al -= 0x60;
		context.flags.template set_from<Flag::Carry>(1);
	}

	context.flags.template set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(al);
}


}

#endif /* BCD_h */
