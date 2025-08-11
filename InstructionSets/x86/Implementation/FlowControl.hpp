//
//  FlowControl.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Resolver.hpp"
#include "Stack.hpp"
#include "InstructionSets/x86/TaskStateSegment.hpp"
#include "InstructionSets/x86/AccessType.hpp"

#include <type_traits>

namespace InstructionSet::x86::Primitive {

template <typename IntT, typename ContextT>
void jump(
	const bool condition,
	const IntT displacement,
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
		context.flow_controller.template jump<uint16_t>(uint16_t(context.registers.ip() + displacement));
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loop(
	modify_t<IntT> counter,
	const OffsetT displacement,
	ContextT &context
) {
	--counter;
	if(counter) {
		context.flow_controller.template jump<uint16_t>(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loope(
	modify_t<IntT> counter,
	const OffsetT displacement,
	ContextT &context
) {
	--counter;
	if(counter && context.flags.template flag<Flag::Zero>()) {
		context.flow_controller.template jump<uint16_t>(context.registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename ContextT>
void loopne(
	modify_t<IntT> counter,
	const OffsetT displacement,
	ContextT &context
) {
	--counter;
	if(counter && !context.flags.template flag<Flag::Zero>()) {
		context.flow_controller.template jump<uint16_t>(context.registers.ip() + displacement);
	}
}

template <typename AddressT, typename ContextT>
void call_relative(
	typename std::make_signed<AddressT>::type offset,
	ContextT &context
) {
	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		push<uint16_t, false>(context.registers.ip(), context);
		context.flow_controller.template jump<AddressT>(AddressT(context.registers.ip() + offset));
	} else {
		assert(false);
	}
}

template <typename IntT, typename AddressT, typename ContextT>
void call_absolute(
	read_t<IntT> target,
	ContextT &context
) {
	push<uint16_t, false>(context.registers.ip(), context);
	context.flow_controller.template jump<AddressT>(AddressT(target));
}

template <typename IntT, typename ContextT>
void jump_absolute(
	read_t<IntT> target,
	ContextT &context
) {
	context.flow_controller.template jump<uint16_t>(target);
}

template <typename AddressT, typename ContextT>
void call_far(
	const uint16_t segment,
	const AddressT offset,
	ContextT &context
) {
	context.segments.preauthorise(
		Source::CS,
		offset,
		[&] {
			context.memory.preauthorise_stack_write(sizeof(uint16_t) * 2, sizeof(uint16_t));
			push<uint16_t, true>(context.registers.cs(), context);
			push<uint16_t, true>(context.registers.ip(), context);
			context.flow_controller.template jump<AddressT>(segment, offset);
		},
		[&] (const SegmentDescriptor &descriptor) {
			(void)descriptor;
			printf("TODO: protected mode far call");
		}
	);
}

template <typename AddressT, InstructionType type, typename ContextT>
void call_far(
	const Instruction<type> &instruction,
	ContextT &context
) {
	const Source source_segment = instruction.data_segment();

	AddressT source_address;
	const auto pointer = instruction.destination();
	switch(pointer.source()) {
		default:
		case Source::Immediate:
			call_far(instruction.segment(), instruction.offset(), context);
		return;

		case Source::Indirect:
			source_address = uint16_t(
				address<Source::Indirect, AddressT, AccessType::Read>(instruction, pointer, context)
			);
		break;
		case Source::IndirectNoBase:
			source_address = uint16_t(
				address<Source::IndirectNoBase, AddressT, AccessType::Read>(instruction, pointer, context)
			);
		break;
		case Source::DirectAddress:
			source_address = uint16_t(
				address<Source::DirectAddress, AddressT, AccessType::Read>(instruction, pointer, context)
			);
		break;
	}

//	context.memory.preauthorise_read(source_segment, source_address, sizeof(uint16_t) + sizeof(AddressT));
	const auto offset =
		context.memory.template access<AddressT, AccessType::Read>(source_segment, source_address);
	source_address += 2;
	const auto segment =
		context.memory.template access<uint16_t, AccessType::Read>(source_segment, source_address);

	call_far(segment, offset, context);
}

template <InstructionType type, typename ContextT>
void jump_far(
	const Instruction<type> &instruction,
	ContextT &context
) {
	// TODO: eliminate 16-bit assumption below.
	uint16_t source_address = 0;
	const auto pointer = instruction.destination();
	switch(pointer.source()) {
		default:
		case Source::Immediate:
			context.flow_controller.template jump<uint16_t>(instruction.segment(), instruction.offset());
		return;

		case Source::Indirect:
			source_address = uint16_t(
				address<Source::Indirect, uint16_t, AccessType::Read>(instruction, pointer, context)
			);
		break;
		case Source::IndirectNoBase:
			source_address = uint16_t(
				address<Source::IndirectNoBase, uint16_t, AccessType::Read>(instruction, pointer, context)
			);
		break;
		case Source::DirectAddress:
			source_address = uint16_t(
				address<Source::DirectAddress, uint16_t, AccessType::Read>(instruction, pointer, context)
			);
		break;
	}

	const Source source_segment = instruction.data_segment();
//	context.memory.preauthorise_read(source_segment, source_address, sizeof(uint16_t) * 2);

	const auto offset =
		context.memory.template access<uint16_t, AccessType::Read>(source_segment, source_address);
	source_address += 2;
	const auto segment =
		context.memory.template access<uint16_t, AccessType::Read>(source_segment, source_address);
	context.flow_controller.template jump<uint16_t>(segment, offset);
}

template <typename ContextT>
void iret(
	ContextT &context
) {
	// TODO: all modes other than 16-bit real mode.
	context.memory.preauthorise_stack_read(sizeof(uint16_t) * 3, sizeof(uint16_t));
	const auto ip = pop<uint16_t, true>(context);
	const auto cs = pop<uint16_t, true>(context);
	context.flags.set(pop<uint16_t, true>(context));
	context.flow_controller.template jump<uint16_t>(cs, ip);
}

template <typename InstructionT, typename ContextT>
void ret_near(
	const InstructionT instruction,
	ContextT &context
) {
	const auto ip = pop<uint16_t, false>(context);
	context.registers.sp() += instruction.operand();
	context.flow_controller.template jump<uint16_t>(ip);
}

template <typename InstructionT, typename ContextT>
void ret_far(
	const InstructionT instruction,
	ContextT &context
) {
	context.memory.preauthorise_stack_read(sizeof(uint16_t) * 2, sizeof(uint16_t));
	const auto ip = pop<uint16_t, true>(context);
	const auto cs = pop<uint16_t, true>(context);
	context.registers.sp() += instruction.operand();
	context.flow_controller.template jump<uint16_t>(cs, ip);
}

template <typename ContextT>
void into(
	ContextT &context
) {
	if(context.flags.template flag<Flag::Overflow>()) {
		constexpr auto exception = Exception::exception<Vector::Overflow>();
		if constexpr (uses_8086_exceptions(ContextT::model)) {
			interrupt(exception, context);
		} else {
			throw exception;
		}
	}
}

template <typename IntT, typename AddressT, typename InstructionT, typename ContextT>
void bound(
	const InstructionT &instruction,
	read_t<IntT> destination,
	read_t<AddressT> source,
	ContextT &context
) {
	using sIntT = typename std::make_signed<IntT>::type;

	const auto source_segment = instruction.data_segment();
	const auto lower_bound =
		sIntT(context.memory.template access<IntT, AccessType::Read>(source_segment, source));
	const auto upper_bound =
		sIntT(context.memory.template access<IntT, AccessType::Read>(source_segment, IntT(source + 2)));

	if(sIntT(destination) < lower_bound || sIntT(destination) > upper_bound) {
		constexpr auto exception = Exception::exception<Vector::BoundRangeExceeded>();
		if constexpr (uses_8086_exceptions(ContextT::model)) {
			interrupt(exception, context);
		} else {
			throw exception;
		}
	}
}

}
