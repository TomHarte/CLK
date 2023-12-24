//
//  LoadStore.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef LoadStore_h
#define LoadStore_h

#include "../AccessType.hpp"

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

template <typename IntT, typename InstructionT, typename ContextT>
void lea(
	const InstructionT &instruction,
	write_t<IntT> destination,
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
void mov(
	write_t<IntT> destination,
	read_t<IntT> source
) {
	destination = source;
}

}

#endif /* LoadStore_h */
