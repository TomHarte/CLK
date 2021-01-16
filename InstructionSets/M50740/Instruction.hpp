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
	Implied,
	Accumulator,
	Immediate,
	Absolute,
	AbsoluteX,
	AbsoluteY,
	ZeroPage,
	ZeroPageX,
	ZeroPageY,
	XIndirect,
	IndirectY,
	Relative,
	AbsoluteIndirect,
	ZeroPageIndirect,
	SpecialPage,
	BitAccumulator,
	BitZeroPage
};

enum class Operation: uint8_t {
	ADC,
	AND,
	ASL,
	BBC,
	BBS,
	BCC,
	BCS,
	BEQ,
	BIT,
	BMI,
	BNE,
	BPL,
	BRA,
	BRK,
	BVC,
	BVS,
	CLB,
	CLC,
	CLD,
	CLI,
	CLT,
	CLV,
	CMP,
	COM,
	CPX,
	CPY,
	DEC,
	DEX,
	DEY,
	EOR,
	FST,
	INC,
	INX,
	INY,
	JMP,
	JSR,
	LDA,
	LDM,
	LDX,
	LDY,
	LSR,
	NOP,
	ORA,
	PHA,
	PHP,
	PLA,
	PLP,
	ROL,
	ROR,
	RRF,
	RTI,
	RTS,
	SBC,
	SEB,
	SEC,
	SED,
	SEI,
	SET,
	SLW,
	STA,
	STP,
	STX,
	STY,
	TAX,
	TAY,
	TST,
	TSX,
	TXS,
	TYA
};

struct Instruction {
	Operation operation;
	AddressingMode addressing_mode;

	Instruction(Operation operation, AddressingMode addressing_mode) : operation(operation), addressing_mode(addressing_mode) {}
};

}
}


#endif /* InstructionSets_M50740_Instruction_h */
