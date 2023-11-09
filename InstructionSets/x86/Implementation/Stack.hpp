//
//  Stack.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Stack_hpp
#define Stack_hpp

#include "../AccessType.hpp"

namespace InstructionSet::x86::Primitive {

// The below takes a reference in order properly to handle PUSH SP,
// which should place the value of SP after the push onto the stack.
template <typename IntT, bool preauthorised, typename ContextT>
void push(
	IntT &value,
	ContextT &context
) {
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
IntT pop(
	ContextT &context
) {
	const auto value = context.memory.template access<IntT, preauthorised ? AccessType::PreauthorisedRead : AccessType::Read>(
		Source::SS,
		context.registers.sp_);
	context.registers.sp_ += sizeof(IntT);
	return value;
}

template <typename ContextT>
void sahf(
	uint8_t &ah,
	ContextT &context
) {
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
void lahf(
	uint8_t &ah,
	ContextT &context
) {
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

template <typename ContextT>
void popf(
	ContextT &context
) {
	context.flags.set(pop<uint16_t, false>(context));
}

template <typename ContextT>
void pushf(
	ContextT &context
) {
	uint16_t value = context.flags.get();
	push<uint16_t, false>(value, context);
}

}

#endif /* Stack_hpp */
