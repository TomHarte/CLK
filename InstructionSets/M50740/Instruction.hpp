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
#include "../AccessType.hpp"

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

constexpr int size(AddressingMode mode) {
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

	// Operations that don't access memory.
	BBC,	BBS,	BCC,	BCS,
	BEQ,	BMI,	BNE,	BPL,
	BVC,	BVS,	BRA,	BRK,
	JMP,	JSR,
	RTI,	RTS,
	CLC,	CLD,	CLI,	CLT,	CLV,
	SEC,	SED,	SEI,	SET,
	INX,	INY,	DEX,	DEY,
	FST,	SLW,
	NOP,
	PHA, 	PHP, 	PLA,	PLP,
	STP,
	TAX,	TAY,	TSX,	TXA,
	TXS,	TYA,

	// Read operations.
	ADC,	SBC,
	AND,	ORA,	EOR,	BIT,
	CMP,	CPX,	CPY,
	LDA,	LDX,	LDY,
	TST,

	// Read-modify-write operations.
	ASL,	LSR,
	CLB,	SEB,
	COM,
	DEC,	INC,
	ROL,	ROR,	RRF,

	// Write operations.
	LDM,
	STA,	STX,	STY,
};

constexpr AccessType access_type(Operation operation) {
	if(operation < Operation::ADC)	return AccessType::None;
	if(operation < Operation::LDM)	return AccessType::Read;
	if(operation < Operation::LDM)	return AccessType::Write;
	return AccessType::ReadModifyWrite;
}

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
