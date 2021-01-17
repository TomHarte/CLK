//
//  Executor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Executor.hpp"

using namespace InstructionSet::M50740;

template <Operation operation> void Executor::perform(uint8_t *operand [[maybe_unused]]) {
}

template <Operation operation, AddressingMode addressing_mode> void Executor::perform(Executor *) {
}

Action Executor::action_for(Instruction instruction) {
	return Action(performer_lookup_.performer(instruction.operation, instruction.addressing_mode));
}
