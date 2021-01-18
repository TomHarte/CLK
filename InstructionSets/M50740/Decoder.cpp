//
//  Decoder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"

#include <algorithm>

namespace InstructionSet {
namespace M50740 {

Instruction Decoder::instrucion_for_opcode(uint8_t opcode) {
	switch(opcode) {
		default:	return Instruction(opcode);

#define Map(opcode, operation, addressing_mode)	case opcode: return Instruction(Operation::operation, AddressingMode::addressing_mode, opcode);

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

	/* 0x40 – 0x4f */
		Map(0x40, RTI, Implied);					Map(0x41, EOR, XIndirect);
		Map(0x42, STP, Implied);					Map(0x43, BBS, Bit2AccumulatorRelative);

		Map(0x44, COM, ZeroPage);					Map(0x45, EOR, ZeroPage);
		Map(0x46, LSR, ZeroPage);					Map(0x47, BBS, Bit2ZeroPageRelative);

		Map(0x48, PHA, Implied);					Map(0x49, EOR, Immediate);
		Map(0x4a, LSR, Accumulator);				Map(0x4b, SEB, Bit2Accumulator);

		Map(0x4c, JMP, Absolute);					Map(0x4d, EOR, Absolute);
		Map(0x4e, LSR, Absolute);					Map(0x4f, SEB, Bit2ZeroPage);

	/* 0x50 – 0x5f */
		Map(0x50, BVC, Relative);					Map(0x51, EOR, IndirectY);
													Map(0x53, BBC, Bit2AccumulatorRelative);

													Map(0x55, EOR, ZeroPageX);
		Map(0x56, LSR, ZeroPageX);					Map(0x57, BBC, Bit2ZeroPageRelative);

		Map(0x58, CLI, Implied);					Map(0x59, EOR, AbsoluteY);
													Map(0x5b, CLB, Bit2Accumulator);

													Map(0x5d, EOR, AbsoluteX);
		Map(0x5e, LSR, AbsoluteX);					Map(0x5f, CLB, Bit2ZeroPage);

	/* 0x60 – 0x6f */
		Map(0x60, RTS, Implied);					Map(0x61, ADC, XIndirect);
													Map(0x63, BBS, Bit3AccumulatorRelative);

		Map(0x64, TST, ZeroPage);					Map(0x65, ADC, ZeroPage);
		Map(0x66, ROR, ZeroPage);					Map(0x67, BBS, Bit3ZeroPageRelative);

		Map(0x68, PLA, Implied);					Map(0x69, ADC, Immediate);
		Map(0x6a, ROR, Accumulator);				Map(0x6b, SEB, Bit3Accumulator);

		Map(0x6c, JMP, AbsoluteIndirect);			Map(0x6d, ADC, Absolute);
		Map(0x6e, ROR, Absolute);					Map(0x6f, SEB, Bit3ZeroPage);

	/* 0x70 – 0x7f */
		Map(0x70, BVS, Relative);					Map(0x71, ADC, IndirectY);
													Map(0x73, BBC, Bit3AccumulatorRelative);

													Map(0x75, ADC, ZeroPageX);
		Map(0x76, ROR, ZeroPageX);					Map(0x77, BBC, Bit3ZeroPageRelative);

		Map(0x78, SEI, Implied);					Map(0x79, ADC, AbsoluteY);
													Map(0x7b, CLB, Bit3Accumulator);

													Map(0x7d, ADC, AbsoluteX);
		Map(0x7e, ROR, AbsoluteX);					Map(0x7f, CLB, Bit3ZeroPage);

	/* 0x80 – 0x8f */
		Map(0x80, BRA, Relative);					Map(0x81, STA, XIndirect);
		Map(0x82, RRF, ZeroPage);					Map(0x83, BBS, Bit4AccumulatorRelative);

		Map(0x84, STY, ZeroPage);					Map(0x85, STA, ZeroPage);
		Map(0x86, STX, ZeroPage);					Map(0x87, BBS, Bit4ZeroPageRelative);

		Map(0x88, DEY, Implied);
		Map(0x8a, TXA, Implied);					Map(0x8b, SEB, Bit4Accumulator);

		Map(0x8c, STY, Absolute);					Map(0x8d, STA, Absolute);
		Map(0x8e, STX, Absolute);					Map(0x8f, SEB, Bit4ZeroPage);

	/* 0x90 – 0x9f */
		Map(0x90, BCC, Relative);					Map(0x91, STA, IndirectY);
													Map(0x93, BBC, Bit4AccumulatorRelative);

		Map(0x94, STY, ZeroPageX);					Map(0x95, STA, ZeroPageX);
		Map(0x96, STX, ZeroPageX);					Map(0x97, BBC, Bit4ZeroPageRelative);

		Map(0x98, TYA, Implied);					Map(0x99, STA, AbsoluteY);
		Map(0x9a, TXS, AbsoluteY);					Map(0x9b, CLB, Bit4Accumulator);

													Map(0x9d, ADC, AbsoluteX);
													Map(0x9f, CLB, Bit4ZeroPage);

	/* 0xa0 – 0xaf */
		Map(0xa0, LDY, Immediate);					Map(0xa1, LDA, XIndirect);
		Map(0xa2, LDX, Immediate);					Map(0xa3, BBS, Bit5AccumulatorRelative);

		Map(0xa4, LDY, ZeroPage);					Map(0xa5, LDA, ZeroPage);
		Map(0xa6, LDX, ZeroPage);					Map(0xa7, BBS, Bit5ZeroPageRelative);

		Map(0xa8, TAY, Implied);					Map(0xa9, LDA, Immediate);
		Map(0xaa, TAX, Implied);					Map(0xab, SEB, Bit5Accumulator);

		Map(0xac, LDY, Absolute);					Map(0xad, LDA, Absolute);
		Map(0xae, LDX, Absolute);					Map(0xaf, SEB, Bit5ZeroPage);

	/* 0xb0 – 0xbf */
		Map(0xb0, BCS, Relative);					Map(0xb1, STA, IndirectY);
		Map(0xb2, JMP, ZeroPageIndirect);			Map(0xb3, BBC, Bit5AccumulatorRelative);

		Map(0xb4, LDY, ZeroPageX);					Map(0xb5, LDA, ZeroPageX);
		Map(0xb6, LDX, ZeroPageY);					Map(0xb7, BBC, Bit5ZeroPageRelative);

		Map(0xb8, CLV, Implied);					Map(0xb9, LDA, AbsoluteY);
		Map(0xba, TSX, AbsoluteY);					Map(0xbb, CLB, Bit5Accumulator);

		Map(0xbc, LDY, AbsoluteX);					Map(0xbd, LDA, AbsoluteX);
		Map(0xbe, LDX, AbsoluteY);					Map(0xbf, CLB, Bit5ZeroPage);

	/* 0xc0 – 0xcf */
		Map(0xc0, CPY, Immediate);					Map(0xc1, CMP, XIndirect);
		Map(0xc2, SLW, Implied);					Map(0xc3, BBS, Bit6AccumulatorRelative);

		Map(0xc4, CPY, ZeroPage);					Map(0xc5, CMP, ZeroPage);
		Map(0xc6, DEC, ZeroPage);					Map(0xc7, BBS, Bit6ZeroPageRelative);

		Map(0xc8, INY, Implied);					Map(0xc9, CMP, Immediate);
		Map(0xca, DEX, Implied);					Map(0xcb, SEB, Bit6Accumulator);

		Map(0xcc, CPY, Absolute);					Map(0xcd, CMP, Absolute);
		Map(0xce, DEC, Absolute);					Map(0xcf, SEB, Bit6ZeroPage);

	/* 0xd0 – 0xdf */
		Map(0xd0, BNE, Relative);					Map(0xd1, CMP, IndirectY);
													Map(0xd3, BBC, Bit6AccumulatorRelative);

													Map(0xd5, CMP, ZeroPageX);
		Map(0xd6, DEC, ZeroPageX);					Map(0xd7, BBC, Bit6ZeroPageRelative);

		Map(0xd8, CLD, Implied);					Map(0xd9, CMP, AbsoluteY);
													Map(0xdb, CLB, Bit6Accumulator);

													Map(0xdd, CMP, AbsoluteX);
		Map(0xde, DEC, AbsoluteX);					Map(0xdf, CLB, Bit6ZeroPage);

	/* 0xe0 – 0xef */
		Map(0xe0, CPX, Immediate);					Map(0xe1, SBC, XIndirect);
		Map(0xe2, FST, Implied);					Map(0xe3, BBS, Bit7AccumulatorRelative);

		Map(0xe4, CPX, ZeroPage);					Map(0xe5, SBC, ZeroPage);
		Map(0xe6, INC, ZeroPage);					Map(0xe7, BBS, Bit7ZeroPageRelative);

		Map(0xe8, INX, Implied);					Map(0xe9, SBC, Immediate);
		Map(0xea, NOP, Implied);					Map(0xeb, SEB, Bit7Accumulator);

		Map(0xec, CPX, Absolute);					Map(0xed, SBC, Absolute);
		Map(0xee, INC, Absolute);					Map(0xef, SEB, Bit7ZeroPage);

	/* 0xf0 – 0xff */
		Map(0xf0, BEQ, Relative);					Map(0xf1, SBC, IndirectY);
													Map(0xf3, BBC, Bit7AccumulatorRelative);

													Map(0xf5, SBC, ZeroPageX);
		Map(0xf6, INC, ZeroPageX);					Map(0xf7, BBC, Bit7ZeroPageRelative);

		Map(0xf8, SED, Implied);					Map(0xf9, SBC, AbsoluteY);
													Map(0xfb, CLB, Bit7Accumulator);

													Map(0xfd, SBC, AbsoluteX);
		Map(0xfe, INC, AbsoluteX);					Map(0xff, CLB, Bit7ZeroPage);

#undef Map
	}
}

std::pair<int, InstructionSet::M50740::Instruction> Decoder::decode(const uint8_t *source, size_t length) {
	const uint8_t *const end = source + length;

	if(phase_ == Phase::Instruction && source != end) {
		const uint8_t instruction = *source;
		++source;
		++consumed_;

		// Determine the instruction in hand, and finish now if its undefined.
		instr_ = instrucion_for_opcode(instruction);
		if(instr_.operation == Operation::Invalid) {
			consumed_ = 0;
			return std::make_pair(1, instr_);
		}

		// Obtain an operand size and roll onto the correct phase.
		operand_size_ = size(instr_.addressing_mode);
		phase_ = operand_size_ ? Phase::AwaitingOperand : Phase::ReadyToPost;
		operand_bytes_ = 0;
	}

	if(phase_ == Phase::AwaitingOperand && source != end) {
		const int outstanding_bytes = operand_size_ - operand_bytes_;
		const int bytes_to_consume = std::min(int(end - source), outstanding_bytes);

		consumed_ += bytes_to_consume;
		source += bytes_to_consume;
		operand_bytes_ += bytes_to_consume;

		if(operand_size_ == operand_bytes_) {
			phase_ = Phase::ReadyToPost;
		} else {
			return std::make_pair(-(operand_size_ - operand_bytes_), Instruction());
		}
	}

	if(phase_ == Phase::ReadyToPost) {
		const auto result = std::make_pair(consumed_, instr_);
		consumed_ = 0;
		phase_ = Phase::Instruction;
		return result;
	}

	// Decoding didn't complete, without it being clear how many more bytes are required.
	return std::make_pair(0, Instruction());
}

}
}
