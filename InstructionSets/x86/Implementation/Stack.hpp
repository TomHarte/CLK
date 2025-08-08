//
//  Stack.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/AccessType.hpp"

#include <type_traits>

namespace InstructionSet::x86::Primitive {

// The below takes a reference in order properly to handle PUSH SP,
// which should place the value of SP after the push onto the stack.
template <typename IntT, bool preauthorised, typename ContextT>
void push(
	const IntT &value,
	ContextT &context
) {
	context.registers.sp() -= sizeof(IntT);
	if constexpr (preauthorised) {
		context.memory.template preauthorised_write<IntT>(Source::SS, context.registers.sp(), value);
	} else {
		context.memory.template access<IntT, AccessType::Write>(
			Source::SS,
			context.registers.sp()) = value;
	}
	context.memory.template write_back<IntT>();
}

template <typename IntT, bool preauthorised, typename ContextT>
IntT pop(
	ContextT &context
) {
	const auto value = context.memory.template access<IntT, preauthorised ? AccessType::PreauthorisedRead : AccessType::Read>(
		Source::SS,
		context.registers.sp());
	context.registers.sp() += sizeof(IntT);
	return value;
}

template <typename ContextT>
void sahf(
	const uint8_t &ah,
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
	const uint16_t value = context.flags.get();
	push<uint16_t, false>(value, context);
}

template <typename IntT, typename ContextT>
void popa(
	ContextT &context
) {
	context.memory.preauthorise_stack_read(8 * sizeof(IntT), sizeof(IntT));
	if constexpr (std::is_same_v<IntT, uint32_t>) {
		context.registers.edi() = pop<uint32_t, true>(context);
		context.registers.esi() = pop<uint32_t, true>(context);
		context.registers.ebp() = pop<uint32_t, true>(context);
		context.registers.esp() += 4;
		context.registers.ebx() = pop<uint32_t, true>(context);
		context.registers.edx() = pop<uint32_t, true>(context);
		context.registers.ecx() = pop<uint32_t, true>(context);
		context.registers.eax() = pop<uint32_t, true>(context);
	} else {
		context.registers.di() = pop<uint16_t, true>(context);
		context.registers.si() = pop<uint16_t, true>(context);
		context.registers.bp() = pop<uint16_t, true>(context);
		context.registers.sp() += 2;
		context.registers.bx() = pop<uint16_t, true>(context);
		context.registers.dx() = pop<uint16_t, true>(context);
		context.registers.cx() = pop<uint16_t, true>(context);
		context.registers.ax() = pop<uint16_t, true>(context);
	}
}

template <typename IntT, typename ContextT>
void pusha(
	ContextT &context
) {
	// Don't preauthorise, as the 286 does write all the intermediate values prior
	// to discovering the fault.
	const IntT initial_sp = context.registers.sp();
	const auto do_pushes = [&] {
		if constexpr (std::is_same_v<IntT, uint32_t>) {
			push<uint32_t, false>(context.registers.eax(), context);
			push<uint32_t, false>(context.registers.ecx(), context);
			push<uint32_t, false>(context.registers.edx(), context);
			push<uint32_t, false>(context.registers.ebx(), context);
			push<uint32_t, false>(initial_sp, context);
			push<uint32_t, false>(context.registers.ebp(), context);
			push<uint32_t, false>(context.registers.esi(), context);
			push<uint32_t, false>(context.registers.edi(), context);
		} else {
			push<uint16_t, false>(context.registers.ax(), context);
			push<uint16_t, false>(context.registers.cx(), context);
			push<uint16_t, false>(context.registers.dx(), context);
			push<uint16_t, false>(context.registers.bx(), context);
			push<uint16_t, false>(initial_sp, context);
			push<uint16_t, false>(context.registers.bp(), context);
			push<uint16_t, false>(context.registers.si(), context);
			push<uint16_t, false>(context.registers.di(), context);
		}
	};

	if(!uses_8086_exceptions(ContextT::model)) {
		try {
			do_pushes();
		} catch (const Exception &e) {
			context.registers.sp() = initial_sp;
			throw e;
		};
	} else {
		do_pushes();
	}
}

template <typename IntT, typename InstructionT, typename ContextT>
void enter(
	const InstructionT &instruction,
	ContextT &context
) {
	// TODO: all non-16bit address sizes.
	const auto alloc_size = instruction.dynamic_storage_size();
	const auto nesting_level = instruction.nesting_level() & 0x1f;

	// Push BP and grab the end of frame.
	const auto original_sp = context.registers.sp();
	const auto original_bp = context.registers.bp();
	const auto do_enter = [&] {
		push<uint16_t, false>(context.registers.bp(), context);
		const auto frame = context.registers.sp();

		// Copy data as per the nesting level.
		if(nesting_level > 0) {
			for(int c = 1; c < nesting_level; c++) {
				context.registers.bp() -= 2;

				const auto value =
					context.memory.template access
						<uint16_t, AccessType::Read>(Source::SS, context.registers.bp());
				push<uint16_t, false>(value, context);
			}
			push<uint16_t, false>(frame, context);
		}

		// Set final BP.
		context.registers.bp() = frame;
		context.registers.sp() -= alloc_size;
	};

	if(!uses_8086_exceptions(ContextT::model)) {
		try {
			do_enter();
		} catch (const Exception &e) {
			context.registers.sp() = original_sp;
			context.registers.bp() = original_bp;
			throw e;
		}
	} else {
		do_enter();
	}

//	assert(final_sp == context.registers.sp());
}

template <typename IntT, typename ContextT>
void leave(
	ContextT &context
) {
	// TODO: should use StackAddressSize to determine assignment of bp to sp.
	if constexpr (std::is_same_v<IntT, uint32_t>) {
		context.memory.preauthorise_read(Source::SS, context.registers.ebp(), sizeof(uint32_t));
		context.registers.esp() = context.registers.ebp();
		context.registers.ebp() = pop<uint32_t, true>(context);
	} else {
		context.memory.preauthorise_read(Source::SS, context.registers.bp(), sizeof(uint16_t));
		context.registers.sp() = context.registers.bp();
		context.registers.bp() = pop<uint16_t, true>(context);
	}
}

}
