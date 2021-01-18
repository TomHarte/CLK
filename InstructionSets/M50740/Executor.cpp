//
//  Executor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Executor.hpp"

using namespace InstructionSet::M50740;

Executor::Executor() {
	// Cut down the list of all generated performers to those the processor actually uses, and install that
	// for future referencing by action_for.
	Decoder decoder;
	for(size_t c = 0; c < 256; c++) {
		const auto instruction = decoder.instrucion_for_opcode(uint8_t(c));
		performers_[c] = performer_lookup_.performer(instruction.operation, instruction.addressing_mode);
	}
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform() {
	// Deal with all modes that don't access memory up here;
	// those that access memory will go through a slightly longer
	// sequence below that wraps the address and checks whether
	// a write is valid [if required].

	int address;
#define next8()		memory_[(program_counter_ + 1) & 0x1fff]
#define next16()	memory_[(program_counter_ + 1) & 0x1fff] | (memory_[(program_counter_ + 2) & 0x1fff] << 8)

	switch(addressing_mode) {

		// Addressing modes with no further memory access.

			case AddressingMode::Implied:
				perform<operation>(nullptr);
				++program_counter_;
			return;

			case AddressingMode::Accumulator:
				perform<operation>(&a_);
				++program_counter_;
			return;

			case AddressingMode::Immediate:
				perform<operation>(&next8());
				program_counter_ += 2;
			return;

		// Addressing modes with a memory access.

			case AddressingMode::Absolute:
				address = next16();
				program_counter_ += 3;
			break;

			case AddressingMode::AbsoluteX:
				address = next16() + x_;
				program_counter_ += 3;
			break;

			case AddressingMode::AbsoluteY:
				address = next16() + y_;
				program_counter_ += 3;
			break;

			/* TODO: the rest. */

			default:
				assert(false);
	}

#undef next16
#undef next8

	assert(access_type(operation) != AccessType::None);

	if constexpr(access_type(operation) == AccessType::Read) {
		perform<operation>(&memory_[address & 0x1fff]);
		return;
	}

	uint8_t value = memory_[address & 0x1fff];
	perform<operation>(&value);

	// TODO: full writing logic here; only the first 96 bytes are RAM,
	// there are also timers and IO ports to handle.
	memory_[address & 0x1fff] = value;
}

template <Operation operation> void Executor::perform(uint8_t *operand [[maybe_unused]]) {
}
