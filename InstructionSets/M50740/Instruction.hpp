//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/15/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M50740_Instruction_h
#define InstructionSets_M50740_Instruction_h

#include <cstdint>

namespace InstructionSet {
namespace M50740 {

enum class AddressingMode {
	Implied,			Accumulator,			Immediate,
	Absolute,			AbsoluteX,				AbsoluteY,
	ZeroPage,			ZeroPageX,				ZeroPageY,
	XIndirect,			IndirectY,
	Relative,
	AbsoluteIndirect,	ZeroPageIndirect,
	SpecialPage,
	ImmediateZeroPage,

	Bit0Accumulator,			Bit1Accumulator,			Bit2Accumulator,			Bit3Accumulator,
	Bit4Accumulator,			Bit5Accumulator,			Bit6Accumulator,			Bit7Accumulator,

	Bit0ZeroPage,				Bit1ZeroPage,				Bit2ZeroPage,				Bit3ZeroPage,
	Bit4ZeroPage,				Bit5ZeroPage,				Bit6ZeroPage,				Bit7ZeroPage,

	Bit0AccumulatorRelative,	Bit1AccumulatorRelative,	Bit2AccumulatorRelative,	Bit3AccumulatorRelative,
	Bit4AccumulatorRelative,	Bit5AccumulatorRelative,	Bit6AccumulatorRelative,	Bit7AccumulatorRelative,

	Bit0ZeroPageRelative,		Bit1ZeroPageRelative,		Bit2ZeroPageRelative,		Bit3ZeroPageRelative,
	Bit4ZeroPageRelative,		Bit5ZeroPageRelative,		Bit6ZeroPageRelative,		Bit7ZeroPageRelative,
};

inline int size(AddressingMode mode) {
	// This is coupled to the AddressingMode list above; be careful!
	constexpr int sizes[] = {
		0, 0, 0,
		2, 2, 2,
		1, 1, 1,
		1, 1,
		1,
		2, 1,
		1,
		2,
		0,	0,	0,	0,
		0,	0,	0,	0,
		1,	1,	1,	1,
		1,	1,	1,	1,
		1,	1,	1,	1,
		1,	1,	1,	1,
		2,	2,	2,	2,
		2,	2,	2,	2,
	};
	return sizes[int(mode)];
}

enum class Operation: uint8_t {
	Invalid,

	ADC,	AND,	ASL,	BBC,
	BBS,	BCC,	BCS,	BEQ,
	BIT,	BMI,	BNE,	BPL,
	BRA,	BRK,	BVC,	BVS,
	CLB,	CLC,	CLD,	CLI,
	CLT,	CLV,	CMP,	COM,
	CPX,	CPY,	DEC,	DEX,
	DEY,	EOR,	FST,	INC,
	INX,	INY,	JMP,	JSR,
	LDA,	LDM,	LDX,	LDY,
	LSR,	NOP,	ORA,	PHA,
	PHP,	PLA,	PLP,	ROL,
	ROR,	RRF,	RTI,	RTS,
	SBC,	SEB,	SEC,	SED,
	SEI,	SET,	SLW,	STA,
	STP,	STX,	STY,	TAX,
	TAY,	TST,	TSX,	TXA,
	TXS,	TYA
};

struct Instruction {
	Operation operation = Operation::Invalid;
	AddressingMode addressing_mode = AddressingMode::Implied;

	Instruction(Operation operation, AddressingMode addressing_mode) : operation(operation), addressing_mode(addressing_mode) {}
	Instruction(Operation operation) : operation(operation) {}
	Instruction() {}
};

}
}


#endif /* InstructionSets_M50740_Instruction_h */
