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

template <Operation operation> void Executor::perform(uint8_t *operand [[maybe_unused]]) {
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform(Executor *) {
}
