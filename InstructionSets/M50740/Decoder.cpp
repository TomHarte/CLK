//
//  Decoder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/15/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"

namespace InstructionSet {
namespace M50740 {

std::pair<int, InstructionSet::M50740::Instruction> Decoder::decode(const uint8_t *source, size_t length) {
	const uint8_t *const end = source + length;

	if(phase_ == Phase::Instruction && source != end) {
		const uint8_t instruction = *source;
		++source;
		++consumed_;

		switch(instruction) {
			default:
			return std::make_pair(1, Instruction());

#define Map(opcode, operation, addressing_mode)	case opcode: instr_ = Instruction(Operation::operation, AddressingMode::addressing_mode); break
			Map(0x00, BRK, Implied);					Map(0x01, ORA, XIndirect);
			Map(0x02, JSR, ZeroPageIndirect);			Map(0x03, BBS, Bit0AccumulatorRelative);

														Map(0x05, ORA, ZeroPage);
			Map(0x06, ASL, ZeroPage);					Map(0x07, BBS, Bit0ZeroPageRelative);

			Map(0x08, PHP, Implied);					Map(0x09, ORA, Immediate);
			Map(0x0a, ASL, Accumulator);				Map(0x0b, SEB, Bit0Accumulator);

														Map(0x0d, ORA, Absolute);
			Map(0x0e, ASL, Absolute);					Map(0x0f, SEB, Bit0ZeroPage);

#undef Map
		}
	}

	return std::make_pair(0, Instruction());
}

}
}
