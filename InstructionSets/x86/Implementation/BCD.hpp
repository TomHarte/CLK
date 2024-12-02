//
//  BCD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "../AccessType.hpp"

#include "../../../Numeric/RegisterSizes.hpp"

namespace InstructionSet::x86::Primitive {

/// If @c add is @c true, performs an AAA; otherwise perfoms an AAS.
template <bool add, typename ContextT>
void aaas(
	CPU::RegisterPair16 &ax,
	ContextT &context
) {
	if((ax.halves.low & 0x0f) > 9 || context.flags.template flag<Flag::AuxiliaryCarry>()) {
		if constexpr (add) {
			ax.halves.low += 6;
			++ax.halves.high;
		} else {
			ax.halves.low -= 6;
			--ax.halves.high;
		}
		context.flags.template set_from<Flag::Carry, Flag::AuxiliaryCarry>(1);
	} else {
		context.flags.template set_from<Flag::Carry>(0);
	}
	ax.halves.low &= 0x0f;
}

template <typename ContextT>
void aad(
	CPU::RegisterPair16 &ax,
	const uint8_t imm,
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
	const uint8_t imm,
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

/// If @c add is @c true, performs a DAA; otherwise perfoms a DAS.
template <bool add, typename ContextT>
void daas(
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
		if constexpr (add) al += 0x06; else al -= 0x06;
		context.flags.template set_from<Flag::AuxiliaryCarry>(1);
	}

	if(top_exceeded_threshold || context.flags.template flag<Flag::Carry>()) {
		if constexpr (add) al += 0x60; else al -= 0x60;
		context.flags.template set_from<Flag::Carry>(1);
	}

	context.flags.template set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(al);
}

}
