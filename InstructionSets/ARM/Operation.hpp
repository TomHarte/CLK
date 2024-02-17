//
//  Operation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

namespace InstructionSet::ARM {

enum class Operation {
	AND,	EOR,	SUB,	RSB,
	ADD,	ADC,	SBC,	RSC,
	TST,	TEQ,	CMP,	CMN,
	ORR,	MOV,	BIC,	MVN,

	B,		BL,
	MUL,	MLA,

	SingleDataTransfer,	// TODO: LDR or STR?
	BlockDataTransfer,	// TODO: LDM or STM?
	SoftwareInterrupt,

	CoprocessorDataOperationOrRegisterTransfer,
	CoprocessorDataTransfer,

	Undefined,
};

enum class Condition {
	EQ,	NE,	CS,	CC,
	MI,	PL,	VS,	VC,
	HI,	LS,	GE,	LT,
	GT,	LE,	AL,	NV,
};

}
