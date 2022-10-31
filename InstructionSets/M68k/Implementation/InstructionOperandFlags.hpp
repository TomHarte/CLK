//
//  InstructionOperandFlags.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_68k_InstructionOperandFlags_hpp
#define InstructionSets_68k_InstructionOperandFlags_hpp

namespace InstructionSet {
namespace M68k {

template <Model model, Operation t_operation> constexpr uint8_t operand_flags(Operation r_operation) {
	switch((t_operation != Operation::Undefined) ? t_operation : r_operation) {
		default:
			assert(false);

		//
		//	No operands are fetched or stored.
		//
		//	(which means that source and destination, if they exist,
		//	should be supplied as their effective addresses)
		//
		case Operation::PEA:
		case Operation::JMP:		case Operation::JSR:
		case Operation::MOVEPw:		case Operation::MOVEPl:
		case Operation::TAS:
		case Operation::RTR:		case Operation::RTS:		case Operation::RTE:
		case Operation::RTD:
		case Operation::TRAP:		case Operation::RESET:		case Operation::NOP:
		case Operation::STOP:		case Operation::TRAPV:		case Operation::BKPT:
			return 0;

		//
		// Operand fetch/store status isn't certain just from the operation; this means
		// that further content from an extension word will be required.
		//
		case Operation::MOVESb:		case Operation::MOVESw:		case Operation::MOVESl:
			return 0;

		//
		//	Single-operand read.
		//
		case Operation::MOVEtoSR:	case Operation::MOVEtoCCR:	case Operation::MOVEtoUSP:
		case Operation::ORItoSR:	case Operation::ORItoCCR:
		case Operation::ANDItoSR:	case Operation::ANDItoCCR:
		case Operation::EORItoSR:	case Operation::EORItoCCR:
		case Operation::Bccb:		case Operation::Bccw:		case Operation::Bccl:
		case Operation::BSRb:		case Operation::BSRw:		case Operation::BSRl:
		case Operation::TSTb:		case Operation::TSTw:		case Operation::TSTl:
		case Operation::MOVEMtoMw:	case Operation::MOVEMtoMl:
		case Operation::MOVEMtoRw:	case Operation::MOVEMtoRl:
		case Operation::MOVEtoC:
			return FetchOp1;

		//
		//	Single-operand write.
		//
		case Operation::MOVEfromUSP:
		case Operation::MOVEfromCCR:
		case Operation::MOVEfromC:
			return StoreOp1;

		//
		//	Single-operand read-modify-write.
		//
		case Operation::NBCD:
		case Operation::NOTb:		case Operation::NOTw:		case Operation::NOTl:
		case Operation::NEGb:		case Operation::NEGw:		case Operation::NEGl:
		case Operation::NEGXb:		case Operation::NEGXw:		case Operation::NEGXl:
		case Operation::EXTbtow:	case Operation::EXTwtol:
		case Operation::SWAP:
		case Operation::UNLINK:
		case Operation::ASLm:		case Operation::ASRm:
		case Operation::LSLm:		case Operation::LSRm:
		case Operation::ROLm:		case Operation::RORm:
		case Operation::ROXLm:		case Operation::ROXRm:
		case Operation::Scc:
			return FetchOp1 | StoreOp1;

		//
		//	CLR and MOVE SR, which are model-dependent.
		//
		case Operation::MOVEfromSR:
		case Operation::CLRb:	case Operation::CLRw:	case Operation::CLRl:
			if constexpr (model == Model::M68000) {
				return FetchOp1 | StoreOp1;
			} else {
				return StoreOp1;
			}

		//
		//	Two-operand; read both.
		//
		case Operation::CMPb:	case Operation::CMPw:	case Operation::CMPl:
		case Operation::CMPAw:	case Operation::CMPAl:
		case Operation::CHKw:
		case Operation::BTST:
		case Operation::LINKw:
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
		case Operation::DIVUw:	case Operation::DIVSw:
		case Operation::MULUw:	case Operation::MULSw:
		case Operation::ASLb:	case Operation::ASLw:	case Operation::ASLl:
		case Operation::ASRb:	case Operation::ASRw:	case Operation::ASRl:
		case Operation::LSLb:	case Operation::LSLw:	case Operation::LSLl:
		case Operation::LSRb:	case Operation::LSRw:	case Operation::LSRl:
		case Operation::ROLb:	case Operation::ROLw:	case Operation::ROLl:
		case Operation::RORb:	case Operation::RORw:	case Operation::RORl:
		case Operation::ROXLb:	case Operation::ROXLw:	case Operation::ROXLl:
		case Operation::ROXRb:	case Operation::ROXRw:	case Operation::ROXRl:
		case Operation::BCHG:
		case Operation::BCLR:	case Operation::BSET:
			return FetchOp1 | FetchOp2 | StoreOp2;

		//
		// Two-operand; read both, write source.
		//
		case Operation::DBcc:
			return FetchOp1 | FetchOp2 | StoreOp1;

		//
		//	Two-operand; read both, write both.
		//
		case Operation::EXG:
			return FetchOp1 | FetchOp2 | StoreOp1 | StoreOp2;

		//
		//	Two-operand; just write destination.
		//
		case Operation::LEA:
			return StoreOp2;
	}
}

}
}

#endif /* InstructionSets_68k_InstructionOperandFlags_hpp */
