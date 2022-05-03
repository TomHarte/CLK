//
//  Sequencer.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_68k_Sequencer_hpp
#define InstructionSets_68k_Sequencer_hpp

#include "Instruction.hpp"
#include "Model.hpp"

#include <cassert>

namespace InstructionSet {
namespace M68k {

static constexpr uint8_t FetchOp1	= (1 << 0);
static constexpr uint8_t FetchOp2	= (1 << 1);
static constexpr uint8_t StoreOp1	= (1 << 2);
static constexpr uint8_t StoreOp2	= (1 << 3);

/*!
	Provides a bitfield with a value in the range 0–15 indicating which of the provided operation's
	operands are accessed via standard fetch and store cycles.

	Unusual bus sequences, such as TAS or MOVEM, are not described here.
*/
template <Model model, Operation t_operation = Operation::Undefined> uint8_t operand_flags(Operation r_operation = Operation::Undefined) {
	switch((t_operation != Operation::Undefined) ? t_operation : r_operation) {
		default:
			assert(false);

		//
		//	No operands are fetched or stored.
		//
		case Operation::LEA:
		case Operation::PEA:
			return 0;

		//
		//	Single-operand read.
		//
		case Operation::MOVEtoSR:	case Operation::MOVEtoCCR:	case Operation::MOVEtoUSP:
		case Operation::ORItoSR:	case Operation::ORItoCCR:
		case Operation::ANDItoSR:	case Operation::ANDItoCCR:
		case Operation::EORItoSR:	case Operation::EORItoCCR:
			return FetchOp1;

		//
		//	Single-operand write.
		//
		case Operation::MOVEfromSR:	case Operation::MOVEfromUSP:
			return StoreOp1;

		//
		//	Single-operand read-modify-write.
		//
		case Operation::NBCD:
		case Operation::EXTbtow:	case Operation::EXTwtol:
			return FetchOp1 | StoreOp1;

		//
		//	Two-operand; read both.
		//
		case Operation::CMPb:	case Operation::CMPw:	case Operation::CMPl:
		case Operation::CMPAw:	case Operation::CMPAl:
			return FetchOp1 | FetchOp2;

		//
		//	Two-operand; read source, write dest.
		//
		case Operation::MOVEb: 	case Operation::MOVEw: 	case Operation::MOVEl:
		case Operation::MOVEAw:	case Operation::MOVEAl:
			return FetchOp1 | StoreOp2;

		//
		//	Two-operand; read both, write dest.
		//
		case Operation::ABCD:	case Operation::SBCD:
		case Operation::ADDb: 	case Operation::ADDw: 	case Operation::ADDl:
		case Operation::ADDAw:	case Operation::ADDAl:
		case Operation::ADDXb: 	case Operation::ADDXw: 	case Operation::ADDXl:
		case Operation::SUBb: 	case Operation::SUBw: 	case Operation::SUBl:
		case Operation::SUBAw:	case Operation::SUBAl:
		case Operation::SUBXb: 	case Operation::SUBXw: 	case Operation::SUBXl:
		case Operation::ORb:	case Operation::ORw:	case Operation::ORl:
		case Operation::ANDb:	case Operation::ANDw:	case Operation::ANDl:
		case Operation::EORb:	case Operation::EORw:	case Operation::EORl:
			return FetchOp1 | FetchOp2 | StoreOp2;

		//
		//	Two-operand; read both, write both.
		//
		case Operation::EXG:
			return FetchOp1 | FetchOp2 | StoreOp1 | StoreOp2;
	}
}


}
}

#endif /* InstructionSets_68k_Sequencer_h */
