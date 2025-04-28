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

	context.memory.preauthorise_read(source_segment, source_address, 4);
	destination = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);
	source_address += 2;
	switch(selector) {
		case Source::DS:	context.registers.ds() = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);	break;
		case Source::ES:	context.registers.es() = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(source_segment, source_address);	break;
	}
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
//		context.memory.set_mode(Mode::Protected286);
//		context.segments.set_mode(Mode::Protected286);
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

template <DescriptorTable table, typename AddressT, typename InstructionT, typename ContextT>
void sdt(
	read_t<AddressT> destination_address,
	const InstructionT &instruction,
	ContextT &context
) {
	const auto segment = instruction.data_segment();
	context.memory.preauthorise_write(
		segment,
		destination_address,
		6);

	const auto location = context.registers.template get<table>();
	context.memory.template preauthorised_write<uint16_t>(segment, destination_address, location.limit);
	context.memory.template preauthorised_write<uint32_t>(segment, AddressT(destination_address + 2), location.base);
}

}
