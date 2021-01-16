//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/15/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M50740_Decoder_hpp
#define InstructionSets_M50740_Decoder_hpp

#include "Instruction.hpp"

#include <utility>

namespace InstructionSet {
namespace M50740 {

class Decoder {
	public:
		std::pair<int, Instruction> decode(const uint8_t *source, size_t length);

	private:
		enum class Phase {
			Instruction,
			AwaitingOperand
		} phase_ = Phase::Instruction;
		int operand_size_ = 0;
		int consumed_ = 0;
		Instruction instr_;
};

}
}

#endif /* InstructionSets_M50740_Decoder_hpp */
