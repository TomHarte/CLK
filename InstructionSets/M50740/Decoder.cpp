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
			/* 0x00 – 0x0f */
			Map(0x00, BRK, Implied);					Map(0x01, ORA, XIndirect);
			Map(0x02, JSR, ZeroPageIndirect);			Map(0x03, BBS, Bit0AccumulatorRelative);

														Map(0x05, ORA, ZeroPage);
			Map(0x06, ASL, ZeroPage);					Map(0x07, BBS, Bit0ZeroPageRelative);

			Map(0x08, PHP, Implied);					Map(0x09, ORA, Immediate);
			Map(0x0a, ASL, Accumulator);				Map(0x0b, SEB, Bit0Accumulator);

														Map(0x0d, ORA, Absolute);
			Map(0x0e, ASL, Absolute);					Map(0x0f, SEB, Bit0ZeroPage);

			/* 0x10 – 0x1f */
			Map(0x10, BPL, Relative);					Map(0x11, ORA, IndirectY);
			Map(0x12, CLT, Implied);					Map(0x13, BBC, Bit0AccumulatorRelative);

														Map(0x15, ORA, ZeroPageX);
			Map(0x16, ASL, ZeroPageX);					Map(0x17, BBC, Bit0ZeroPageRelative);

			Map(0x18, CLC, Implied);					Map(0x19, ORA, AbsoluteY);
			Map(0x1a, DEC, Accumulator);				Map(0x1b, CLB, Bit0Accumulator);

														Map(0x1d, ORA, AbsoluteX);
			Map(0x1e, ASL, AbsoluteX);					Map(0x1f, CLB, Bit0ZeroPage);

			/* 0x20 – 0x2f */
			Map(0x20, JSR, Absolute);					Map(0x21, AND, XIndirect);
			Map(0x22, JSR, SpecialPage);				Map(0x23, BBS, Bit1AccumulatorRelative);

			Map(0x24, BIT, ZeroPage);					Map(0x25, AND, ZeroPage);
			Map(0x26, ROL, ZeroPage);					Map(0x27, BBS, Bit1ZeroPageRelative);

			Map(0x28, PLP, Implied);					Map(0x29, AND, Immediate);
			Map(0x2a, ROL, Accumulator);				Map(0x2b, SEB, Bit1Accumulator);

			Map(0x2c, BIT, Absolute);					Map(0x2d, AND, Absolute);
			Map(0x2e, ROL, Absolute);					Map(0x2f, SEB, Bit1ZeroPage);

			/* 0x30 – 0x3f */
			Map(0x30, BMI, Relative);					Map(0x31, AND, IndirectY);
			Map(0x32, SET, Implied);					Map(0x33, BBC, Bit1AccumulatorRelative);

														Map(0x35, AND, ZeroPageX);
			Map(0x36, ROL, ZeroPageX);					Map(0x37, BBC, Bit1ZeroPageRelative);

			Map(0x38, SEC, Implied);					Map(0x39, AND, AbsoluteY);
			Map(0x3a, INC, Accumulator);				Map(0x3b, CLB, Bit1Accumulator);

			Map(0x3c, LDM, ImmediateZeroPage);			Map(0x3d, AND, AbsoluteX);
			Map(0x3e, ROL, AbsoluteX);					Map(0x3f, CLB, Bit1ZeroPage);

#undef Map
		}
	}

	return std::make_pair(0, Instruction());
}

}
}
