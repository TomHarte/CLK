//
//  Logical.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "../AccessType.hpp"

namespace InstructionSet::x86::Primitive {

template <typename IntT, typename ContextT>
void and_(
	modify_t<IntT> destination,
	read_t<IntT> source,
	ContextT &context
) {
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
void or_(
	modify_t<IntT> destination,
	read_t<IntT> source,
	ContextT &context
) {
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
void xor_(
	modify_t<IntT> destination,
	read_t<IntT> source,
	ContextT &context
) {
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

template <typename IntT>
void not_(
	modify_t<IntT> destination
) {
	/*
		DEST ← NOT DEST;
	*/
	/*
		Flags affected: none.
	*/
	destination = ~destination;
}

template <typename IntT>
void cbw(
	IntT &ax
) {
	constexpr IntT test_bit = 1 << (sizeof(IntT) * 4 - 1);
	constexpr IntT low_half = (1 << (sizeof(IntT) * 4)) - 1;

	if(ax & test_bit) {
		ax |= ~low_half;
	} else {
		ax &= low_half;
	}
}

template <typename IntT>
void cwd(
	IntT &dx,
	const IntT ax
) {
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
void salc(
	uint8_t &al,
	ContextT &context
) {
	al = context.flags.template flag<Flag::Carry>() ? 0xff : 0x00;
}

template <typename IntT, typename ContextT>
void setmo(
	write_t<IntT> destination,
	ContextT &context
) {
	const auto result = destination = IntT(~0);
	context.flags.template set_from<Flag::Carry, Flag::AuxiliaryCarry, Flag::Overflow>(0);
	context.flags.template set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(result);
}

}
