//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Decoder.hpp"
#include "Model.hpp"
#include "Operation.hpp"

namespace InstructionSet::ARM {

template <Model model>
class Instruction {
	public:
		constexpr Instruction(uint32_t opcode) noexcept : opcode_(opcode) {}

		Condition condition() const {	return Condition(opcode_ >> 28);	}
		Operation operation() const {
			return InstructionSet::ARM::operation<model>(opcode_);
		}

	private:
		uint32_t opcode_;
};

}
