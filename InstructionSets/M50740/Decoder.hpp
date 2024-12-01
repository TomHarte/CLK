//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Instruction.hpp"

#include <cstddef>
#include <utility>

namespace InstructionSet::M50740 {

class Decoder {
public:
	std::pair<int, Instruction> decode(const uint8_t *, size_t length);
	Instruction instrucion_for_opcode(uint8_t);

private:
	enum class Phase {
		Instruction,
		AwaitingOperand,
		ReadyToPost
	} phase_ = Phase::Instruction;
	int operand_size_ = 0, operand_bytes_ = 0;
	int consumed_ = 0;
	Instruction instr_;
};

}
