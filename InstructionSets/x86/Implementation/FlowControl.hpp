//
//  FlowControl.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef FlowControl_hpp
#define FlowControl_hpp

#include "Resolver.hpp"
#include "Stack.hpp"
#include "../AccessType.hpp"

namespace InstructionSet::x86::Primitive {

template <typename IntT, typename ContextT>
void jump(
	bool condition,
	IntT displacement,
	ContextT &context
) {
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
void loop(
	modify_t<IntT> counter,
	OffsetT displacement,
	ContextT &context
) {
	--counter;
	if(counter) {
		context.flow_controller.jump(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loope(
	modify_t<IntT> counter,
	OffsetT displacement,
	ContextT &context
) {
	--counter;
	if(counter && context.flags.template flag<Flag::Zero>()) {
		context.flow_controller.jump(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loopne(
	modify_t<IntT> counter,
	OffsetT displacement,
	ContextT &context
) {
	--counter;
	if(counter && !context.flags.template flag<Flag::Zero>()) {
		context.flow_controller.jump(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename ContextT>
void call_relative(
	IntT offset,
	ContextT &context
) {
	push<uint16_t, false>(context.registers.ip(), context);
	context.flow_controller.jump(context.registers.ip() + offset);
}

template <typename IntT, typename ContextT>
void call_absolute(
	read_t<IntT> target,
	ContextT &context
) {
	push<uint16_t, false>(context.registers.ip(), context);
	context.flow_controller.jump(target);
}

template <typename IntT, typename ContextT>
void jump_absolute(
	read_t<IntT> target,
	ContextT &context
) {
	context.flow_controller.jump(target);
}

template <typename InstructionT, typename ContextT>
void call_far(
	InstructionT &instruction,
	ContextT &context
) {
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
void jump_far(
	InstructionT &instruction,
	ContextT &context
) {
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
void iret(
	ContextT &context
) {
	// TODO: all modes other than 16-bit real mode.
	context.memory.preauthorise_stack_read(sizeof(uint16_t) * 3);
	const auto ip = pop<uint16_t, true>(context);
	const auto cs = pop<uint16_t, true>(context);
	context.flags.set(pop<uint16_t, true>(context));
	context.flow_controller.jump(cs, ip);
}

template <typename InstructionT, typename ContextT>
void ret_near(
	InstructionT instruction,
	ContextT &context
) {
	const auto ip = pop<uint16_t, false>(context);
	context.registers.sp() += instruction.operand();
	context.flow_controller.jump(ip);
}

template <typename InstructionT, typename ContextT>
void ret_far(
	InstructionT instruction,
	ContextT &context
) {
	context.memory.preauthorise_stack_read(sizeof(uint16_t) * 2);
	const auto ip = pop<uint16_t, true>(context);
	const auto cs = pop<uint16_t, true>(context);
	context.registers.sp() += instruction.operand();
	context.flow_controller.jump(cs, ip);
}

template <typename ContextT>
void into(
	ContextT &context
) {
	if(context.flags.template flag<Flag::Overflow>()) {
		interrupt(Interrupt::OnOverflow, context);
	}
}

}

#endif /* FlowControl_hpp */
