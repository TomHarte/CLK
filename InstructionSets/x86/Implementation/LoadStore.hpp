//
//  LoadStore.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Descriptors.hpp"
#include "InstructionSets/x86/Descriptors.hpp"
#include "InstructionSets/x86/MachineStatus.hpp"

#include <utility>

namespace InstructionSet::x86::Primitive {

template <typename IntT>
void xchg(
	modify_t<IntT> destination,
	modify_t<IntT> source
) {
	/*
		TEMP ← DEST
		DEST ← SRC
		SRC ← TEMP
	*/
	std::swap(destination, source);
}

template <Source selector, typename InstructionT, typename ContextT>
void ld(
	const InstructionT &instruction,
	write_t<uint16_t> destination,
	ContextT &context
) {
	const auto pointer = instruction.source();
	uint16_t source_address = uint16_t(address<uint16_t, AccessType::Read>(instruction, pointer, context));
	const Source source_segment = instruction.data_segment();

	const auto offset =
		context.memory.template access<uint16_t, AccessType::Read>(source_segment, source_address);
	source_address += 2;
	const auto segment =
		context.memory.template access<uint16_t, AccessType::Read>(source_segment, source_address);

	context.segments.preauthorise(selector, segment);
	destination = offset;
	switch(selector) {
		case Source::DS:	context.registers.ds() = segment;	break;
		case Source::ES:	context.registers.es() = segment;	break;
	}
	context.segments.did_update(selector);
}

template <typename IntT, InstructionType type, typename ContextT>
void lea(
	const Instruction<type> &instruction,
	write_t<IntT> destination,
	ContextT &context
) {
	// TODO: address size.
	destination = IntT(address<uint16_t, AccessType::Read, type>(instruction, instruction.source(), context));
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
void mov(
	write_t<IntT> destination,
	read_t<IntT> source
) {
	destination = source;
}

template <typename ContextT>
void smsw(
	write_t<uint16_t> destination,
	ContextT &context
) {
	destination = context.registers.msw();
}

template <typename ContextT>
void lmsw(
	read_t<uint16_t> source,
	ContextT &context
) {
	context.registers.set_msw(source);
	if(source & MachineStatus::ProtectedModeEnable) {
		context.cpu_control.set_mode(Mode::Protected286);
	}
}

template <DescriptorTable table, typename AddressT, typename InstructionT, typename ContextT>
void ldt(
	read_t<AddressT> source_address,
	const InstructionT &instruction,
	ContextT &context
) {
	const auto segment = instruction.data_segment();
	context.memory.preauthorise_read(
		segment,
		source_address,
		6);

	DescriptorTablePointer location;
	location.limit =
		context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(segment, source_address);
	location.base =
		context.memory.template access<uint32_t, AccessType::PreauthorisedRead>(segment, AddressT(source_address + 2));
	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		location.base &= 0xff'ff'ff;
	}

	context.registers.template set<table>(location);
	context.segments.did_update(table);
}

template <typename AddressT, typename ContextT>
void lldt(
	read_t<AddressT> source_segment,
	ContextT &context
) {
	if(!source_segment || context.registers.privilege_level()) {
		throw Exception::exception<Vector::GeneralProtectionFault>(ExceptionCode::zero());
	}

	const auto ldt =
		descriptor_at<SegmentDescriptor>(
			context.linear_memory,
			context.registers.template get<DescriptorTable::Global>(),
			source_segment & ~7,
			false);

	constexpr auto exception_code = [](const uint16_t segment) {
		return ExceptionCode(
			segment &~ 7,
			false,
			false,
			false
		);
	};

	if(ldt.description().type != DescriptorType::LDT) {
		throw Exception::exception<Vector::GeneralProtectionFault>(exception_code(source_segment));
	}

	if(!ldt.present()) {
		throw Exception::exception<Vector::SegmentNotPresent>(exception_code(source_segment));
	}

	DescriptorTablePointer location;
	location.limit = AddressT(ldt.base());
	location.base = ldt.offset();
	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		location.base &= 0xff'ff'ff;
	}

	context.registers.template set<DescriptorTable::Local>(location);
	context.registers.set_ldtr(source_segment);
	context.segments.did_update(DescriptorTable::Local);
}

template <typename AddressT, typename ContextT>
void sldt(
	write_t<AddressT> segment,
	ContextT &context
) {
	segment = AddressT(context.registers.ldtr());
}

template <DescriptorTable table, typename AddressT, typename InstructionT, typename ContextT>
void sdt(
	read_t<AddressT> destination,
	const InstructionT &instruction,
	ContextT &context
) {
	const auto segment = instruction.data_segment();
	context.memory.preauthorise_write(
		segment,
		destination,
		6);

	const auto location = context.registers.template get<table>();
	context.memory.template preauthorised_write<uint16_t>(segment, destination, location.limit);
	context.memory.template preauthorised_write<uint32_t>(segment, AddressT(destination + 2), location.base);
}

template <typename ContextT>
void arpl(
	modify_t<uint16_t> destination,
	read_t<uint16_t> source,
	ContextT &context
) {
	const auto destination_rpl = destination & 3;
	const auto source_rpl = source & 3;

	if(destination_rpl < source_rpl) {
		destination = uint16_t((destination & ~3) | source_rpl);
		context.flags.template set_from<Flag::Zero>(0);
	} else {
		context.flags.template set_from<Flag::Zero>(1);
	}
}

template <typename ContextT>
void clts(
	ContextT &context
) {
	if(context.registers.privilege_level()) {
		throw Exception::exception<Vector::GeneralProtectionFault>(ExceptionCode::zero());
	}

	const auto msw = context.registers.msw();
	context.registers.set_msw(
		msw & ~MachineStatus::TaskSwitched
	);
}

template <typename ContextT>
void ltr(
	read_t<uint16_t> source,
	ContextT &context
) {
	if(context.registers.privilege_level()) {
		throw Exception::exception<Vector::GeneralProtectionFault>(ExceptionCode::zero());
	}

	context.segments.preauthorise_task_state(source);
	context.registers.set_task_state(source);
}

template <typename ContextT>
void str(
	write_t<uint16_t> destination,
	ContextT &context
) {
	destination = context.registers.task_state();
}

template <typename ContextT>
void verr(
	read_t<uint16_t> source,
	ContextT &context
) {
	context.flags.template set_from<Flag::Zero>(!context.segments.template verify<true>(source));
}

template <typename ContextT>
void verw(
	read_t<uint16_t> source,
	ContextT &context
) {
	context.flags.template set_from<Flag::Zero>(!context.segments.template verify<false>(source));
}

}
