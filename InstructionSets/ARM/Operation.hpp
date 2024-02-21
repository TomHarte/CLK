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
	/// Rd = Op1 AND Op2.
	AND,
	/// Rd = Op1 EOR Op2.
	EOR,
	/// Rd = Op1 - Op2.
	SUB,
	/// Rd = Op2 - Op1.
	RSB,
	/// Rd = Op1 + Op2.
	ADD,
	/// Rd = Op1 + Ord2 + C.
	ADC,
	/// Rd = Op1 - Op2 + C.
	SBC,
	/// Rd = Op2 - Op1 + C.
	RSC,
	/// Set condition codes on Op1 AND Op2.
	TST,
	/// Set condition codes on Op1 EOR Op2.
	TEQ,
	/// Set condition codes on Op1 - Op2.
	CMP,
	/// Set condition codes on Op1 + Op2.
	CMN,
	/// Rd = Op1 OR Op2.
	ORR,
	/// Rd = Op2
	MOV,
	/// Rd = Op1 AND NOT Op2.
	BIC,
	/// Rd = NOT Op2.
	MVN,

	MUL,	MLA,
	B,		BL,

	LDR, 	STR,
	LDM,	STM,
	SWI,

	CDP,
	MRC, MCR,
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
