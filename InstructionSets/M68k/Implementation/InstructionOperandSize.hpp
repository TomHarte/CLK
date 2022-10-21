//
//  InstructionOperandSize.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_68k_InstructionOperandSize_hpp
#define InstructionSets_68k_InstructionOperandSize_hpp

namespace InstructionSet {
namespace M68k {

template <Operation t_operation>
constexpr DataSize operand_size(Operation r_operation) {
	switch((t_operation == Operation::Undefined) ? r_operation : t_operation) {
		// These are given a value arbitrarily, to
		// complete the switch statement.
		case Operation::Undefined:
		case Operation::NOP:
		case Operation::STOP:
		case Operation::RESET:
		case Operation::RTE:	case Operation::RTR:
		case Operation::TRAP:
		case Operation::TRAPV:

		case Operation::ABCD:	case Operation::SBCD:
		case Operation::NBCD:
		case Operation::ADDb:	case Operation::ADDXb:
		case Operation::SUBb:	case Operation::SUBXb:
		case Operation::MOVEb:
		case Operation::ORItoCCR:
		case Operation::ANDItoCCR:
		case Operation::EORItoCCR:
		case Operation::BTST:	case Operation::BCLR:
		case Operation::BCHG:	case Operation::BSET:
		case Operation::CMPb:	case Operation::TSTb:
		case Operation::Bccb:	case Operation::BSRb:
		case Operation::CLRb:
		case Operation::Scc:
		case Operation::NEGXb:	case Operation::NEGb:
		case Operation::ASLb:	case Operation::ASRb:
		case Operation::LSLb:	case Operation::LSRb:
		case Operation::ROLb:	case Operation::RORb:
		case Operation::ROXLb:	case Operation::ROXRb:
		case Operation::ANDb:	case Operation::EORb:
		case Operation::NOTb:	case Operation::ORb:
		case Operation::TAS:
			return DataSize::Byte;

		case Operation::ADDw:	case Operation::ADDAw:
		case Operation::ADDXw:	case Operation::SUBw:
		case Operation::SUBAw:	case Operation::SUBXw:
		case Operation::MOVEw:	case Operation::MOVEAw:
		case Operation::ORItoSR:
		case Operation::ANDItoSR:
		case Operation::EORItoSR:
		case Operation::MOVEtoSR:
		case Operation::MOVEfromSR:
		case Operation::MOVEtoCCR:
		case Operation::CMPw:	case Operation::CMPAw:
		case Operation::TSTw:
		case Operation::DBcc:
		case Operation::Bccw:	case Operation::BSRw:
		case Operation::CLRw:
		case Operation::NEGXw:	case Operation::NEGw:
		case Operation::ASLw:	case Operation::ASLm:
		case Operation::ASRw:	case Operation::ASRm:
		case Operation::LSLw:	case Operation::LSLm:
		case Operation::LSRw:	case Operation::LSRm:
		case Operation::ROLw:	case Operation::ROLm:
		case Operation::RORw:	case Operation::RORm:
		case Operation::ROXLw:	case Operation::ROXLm:
		case Operation::ROXRw:	case Operation::ROXRm:
		case Operation::MOVEMtoRw:
		case Operation::MOVEMtoRl:
		case Operation::MOVEMtoMw:
		case Operation::MOVEMtoMl:
		case Operation::MOVEPw:
		case Operation::ANDw:	case Operation::EORw:
		case Operation::NOTw:	case Operation::ORw:
		case Operation::DIVU:	case Operation::DIVS:
		case Operation::MULU:	case Operation::MULS:
		case Operation::EXTbtow:
		case Operation::LINKw:
		case Operation::CHK:
			return DataSize::Word;

		case Operation::ADDl:	case Operation::ADDAl:
		case Operation::ADDXl:	case Operation::SUBl:
		case Operation::SUBAl:	case Operation::SUBXl:
		case Operation::MOVEl:	case Operation::MOVEAl:
		case Operation::LEA:	case Operation::PEA:
		case Operation::EXG:	case Operation::SWAP:
		case Operation::MOVEtoUSP:
		case Operation::MOVEfromUSP:
		case Operation::CMPl:	case Operation::CMPAl:
		case Operation::TSTl:
		case Operation::JMP:	case Operation::JSR:
		case Operation::RTS:
		case Operation::Bccl:	case Operation::BSRl:
		case Operation::CLRl:
		case Operation::NEGXl:	case Operation::NEGl:
		case Operation::ASLl:	case Operation::ASRl:
		case Operation::LSLl:	case Operation::LSRl:
		case Operation::ROLl:	case Operation::RORl:
		case Operation::ROXLl:	case Operation::ROXRl:
		case Operation::MOVEPl:
		case Operation::ANDl:	case Operation::EORl:
		case Operation::NOTl:	case Operation::ORl:
		case Operation::EXTwtol:
		case Operation::UNLINK:
			return DataSize::LongWord;

		default:
			// 68020 TODO.
			return DataSize::Byte;
	}
}

}
}

#endif /* InstructionSets_68k_InstructionOperandSize_hpp */
