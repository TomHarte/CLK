//
//  Instruction.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "Instruction.hpp"

#include <cassert>

using namespace InstructionSet::M68k;

std::string Preinstruction::operand_description(int index, int opcode) const {
	switch(mode(index)) {
		default:	assert(false);

		case AddressingMode::None:
			return "";

		case AddressingMode::DataRegisterDirect:
			return std::string("D") + std::to_string(reg(index));

		case AddressingMode::AddressRegisterDirect:
			return std::string("A") + std::to_string(reg(index));
		case AddressingMode::AddressRegisterIndirect:
			return std::string("(A") + std::to_string(reg(index)) + ")";
		case AddressingMode::AddressRegisterIndirectWithPostincrement:
			return std::string("(A") + std::to_string(reg(index)) + ")+";
		case AddressingMode::AddressRegisterIndirectWithPredecrement:
			return std::string("-(A") + std::to_string(reg(index)) + ")";
		case AddressingMode::AddressRegisterIndirectWithDisplacement:
			return std::string("(d16, A") + std::to_string(reg(index)) + ")";
		case AddressingMode::AddressRegisterIndirectWithIndex8bitDisplacement:
			return std::string("(d8, A") + std::to_string(reg(index)) + ", Xn)";

		case AddressingMode::ProgramCounterIndirectWithDisplacement:
			return "(d16, PC)";
		case AddressingMode::ProgramCounterIndirectWithIndex8bitDisplacement:
			return "(d8, PC, Xn)";

		case AddressingMode::AbsoluteShort:
			return "(xxx).w";
		case AddressingMode::AbsoluteLong:
			return "(xxx).l";

		case AddressingMode::ExtensionWord:
		case AddressingMode::ImmediateData:
			return "#";

		case AddressingMode::Quick:
			if(opcode == -1) {
				return "Q";
			}
			return std::to_string(int(quick(uint16_t(opcode), operation)));
	}
}

namespace {

const char *_to_string(Operation operation, bool is_quick) {
	switch(operation) {
		case Operation::Undefined:		return "None";
		case Operation::NOP:			return "NOP";
		case Operation::ABCD:			return "ABCD";
		case Operation::SBCD:			return "SBCD";
		case Operation::NBCD:			return "NBCD";

		case Operation::ADDb:			return "ADD.b";
		case Operation::ADDw:			return "ADD.w";
		case Operation::ADDl:			return "ADD.l";

		case Operation::ADDAw:			return is_quick ? "ADD.w" : "ADDA.w";
		case Operation::ADDAl:			return is_quick ? "ADD.l" : "ADDA.l";

		case Operation::ADDXb:			return "ADDX.b";
		case Operation::ADDXw:			return "ADDX.w";
		case Operation::ADDXl:			return "ADDX.l";

		case Operation::SUBb:			return "SUB.b";
		case Operation::SUBw:			return "SUB.w";
		case Operation::SUBl:			return "SUB.l";

		case Operation::SUBAw:			return is_quick ? "SUB.w" : "SUBA.w";
		case Operation::SUBAl:			return is_quick ? "SUB.l" : "SUBA.l";

		case Operation::SUBXb:			return "SUBX.b";
		case Operation::SUBXw:			return "SUBX.w";
		case Operation::SUBXl:			return "SUBX.l";

		case Operation::MOVEb:			return "MOVE.b";
		case Operation::MOVEw:			return "MOVE.w";
		case Operation::MOVEl:			return is_quick ? "MOVE.q" : "MOVE.l";

		case Operation::MOVEAw:			return "MOVEA.w";
		case Operation::MOVEAl:			return "MOVEA.l";

		case Operation::MOVESb:			return "MOVES.b";
		case Operation::MOVESw:			return "MOVES.w";
		case Operation::MOVESl:			return "MOVES.l";

		case Operation::LEA:			return "LEA";
		case Operation::PEA:			return "PEA";

		case Operation::MOVEtoSR:		return "MOVEtoSR";
		case Operation::MOVEfromSR:		return "MOVEfromSR";
		case Operation::MOVEtoCCR:		return "MOVEtoCCR";
		case Operation::MOVEfromCCR:	return "MOVEfromCCR";
		case Operation::MOVEtoUSP:		return "MOVEtoUSP";
		case Operation::MOVEfromUSP:	return "MOVEfromUSP";
		case Operation::MOVEtoC:		return "MOVEtoC";
		case Operation::MOVEfromC:		return "MOVEfromC";

		case Operation::ORItoSR:		return "ORItoSR";
		case Operation::ORItoCCR:		return "ORItoCCR";
		case Operation::ANDItoSR:		return "ANDItoSR";
		case Operation::ANDItoCCR:		return "ANDItoCCR";
		case Operation::EORItoSR:		return "EORItoSR";
		case Operation::EORItoCCR:		return "EORItoCCR";

		case Operation::BTST:			return "BTST";
		case Operation::BCLR:			return "BCLR";
		case Operation::BCHG:			return "BCHG";
		case Operation::BSET:			return "BSET";

		case Operation::CMPb:			return "CMP.b";
		case Operation::CMPw:			return "CMP.w";
		case Operation::CMPl:			return "CMP.l";

		case Operation::CMPAw:			return "CMPA.w";
		case Operation::CMPAl:			return "CMPA.l";

		case Operation::TSTb:			return "TST.b";
		case Operation::TSTw:			return "TST.w";
		case Operation::TSTl:			return "TST.l";

		case Operation::JMP:			return "JMP";
		case Operation::JSR:			return "JSR";
		case Operation::RTS:			return "RTS";
		case Operation::RTD:			return "RTD";
		case Operation::RTM:			return "RTM";

		case Operation::DBcc:			return "DBcc";
		case Operation::Scc:			return "Scc";
		case Operation::TRAPcc:			return "TRAPcc";

		case Operation::Bccb:
		case Operation::Bccl:
		case Operation::Bccw:			return "Bcc";

		case Operation::BSRb:
		case Operation::BSRl:
		case Operation::BSRw:			return "BSR";

		case Operation::CASb:			return "CAS.b";
		case Operation::CASw:			return "CAS.w";
		case Operation::CASl:			return "CAS.l";

		case Operation::CLRb:			return "CLR.b";
		case Operation::CLRw:			return "CLR.w";
		case Operation::CLRl:			return "CLR.l";

		case Operation::NEGXb:			return "NEGX.b";
		case Operation::NEGXw:			return "NEGX.w";
		case Operation::NEGXl:			return "NEGX.l";

		case Operation::NEGb:			return "NEG.b";
		case Operation::NEGw:			return "NEG.w";
		case Operation::NEGl:			return "NEG.l";

		case Operation::ASLb:			return "ASL.b";
		case Operation::ASLw:			return "ASL.w";
		case Operation::ASLl:			return "ASL.l";
		case Operation::ASLm:			return "ASL.w";

		case Operation::ASRb:			return "ASR.b";
		case Operation::ASRw:			return "ASR.w";
		case Operation::ASRl:			return "ASR.l";
		case Operation::ASRm:			return "ASR.w";

		case Operation::LSLb:			return "LSL.b";
		case Operation::LSLw:			return "LSL.w";
		case Operation::LSLl:			return "LSL.l";
		case Operation::LSLm:			return "LSL.w";

		case Operation::LSRb:			return "LSR.b";
		case Operation::LSRw:			return "LSR.w";
		case Operation::LSRl:			return "LSR.l";
		case Operation::LSRm:			return "LSR.w";

		case Operation::ROLb:			return "ROL.b";
		case Operation::ROLw:			return "ROL.w";
		case Operation::ROLl:			return "ROL.l";
		case Operation::ROLm:			return "ROL.w";

		case Operation::RORb:			return "ROR.b";
		case Operation::RORw:			return "ROR.w";
		case Operation::RORl:			return "ROR.l";
		case Operation::RORm:			return "ROR.w";

		case Operation::ROXLb:			return "ROXL.b";
		case Operation::ROXLw:			return "ROXL.w";
		case Operation::ROXLl:			return "ROXL.l";
		case Operation::ROXLm:			return "ROXL.w";

		case Operation::ROXRb:			return "ROXR.b";
		case Operation::ROXRw:			return "ROXR.w";
		case Operation::ROXRl:			return "ROXR.l";
		case Operation::ROXRm:			return "ROXR.w";

		case Operation::MOVEMtoMl:		return "MOVEM.l";
		case Operation::MOVEMtoMw:		return "MOVEM.w";
		case Operation::MOVEMtoRl:		return "MOVEM.l";
		case Operation::MOVEMtoRw:		return "MOVEM.w";

		case Operation::MOVEPl:			return "MOVEP.l";
		case Operation::MOVEPw:			return "MOVEP.w";

		case Operation::ANDb:			return "AND.b";
		case Operation::ANDw:			return "AND.w";
		case Operation::ANDl:			return "AND.l";

		case Operation::EORb:			return "EOR.b";
		case Operation::EORw:			return "EOR.w";
		case Operation::EORl:			return "EOR.l";

		case Operation::NOTb:			return "NOT.b";
		case Operation::NOTw:			return "NOT.w";
		case Operation::NOTl:			return "NOT.l";

		case Operation::ORb:			return "OR.b";
		case Operation::ORw:			return "OR.w";
		case Operation::ORl:			return "OR.l";

		case Operation::MULUw:			return "MULU";
		case Operation::MULSw:			return "MULS";
		case Operation::DIVUw:			return "DIVU";
		case Operation::DIVSw:			return "DIVS";

		case Operation::RTE:			return "RTE";
		case Operation::RTR:			return "RTR";

		case Operation::TRAP:			return "TRAP";
		case Operation::TRAPV:			return "TRAPV";
		case Operation::CHKw:			return "CHK";

		case Operation::EXG:			return "EXG";
		case Operation::SWAP:			return "SWAP";

		case Operation::TAS:			return "TAS";

		case Operation::EXTbtow:		return "EXT.w";
		case Operation::EXTwtol:		return "EXT.l";

		case Operation::LINKw:			return "LINK";
		case Operation::UNLINK:			return "UNLINK";

		case Operation::STOP:			return "STOP";
		case Operation::RESET:			return "RESET";

		case Operation::BKPT:			return "BKPT";

		case Operation::BFCHG:			return "BFCHG";
		case Operation::BFCLR:			return "BFCLR";
		case Operation::BFEXTS:			return "BFEXTS";
		case Operation::BFEXTU:			return "BFEXTU";
		case Operation::BFFFO:			return "BFFFO";
		case Operation::BFINS:			return "BFINS";
		case Operation::BFSET:			return "BFSET";
		case Operation::BFTST:			return "BFTST";

		case Operation::PACK:			return "PACK";
		case Operation::UNPK:			return "UNPK";

		default:
			assert(false);
			return "???";
	}
}

}

const char *InstructionSet::M68k::to_string(Operation operation) {
	return _to_string(operation, false);
}

std::string Preinstruction::to_string(int opcode) const {
	if(operation == Operation::Undefined) return "None";

	const char *const instruction = _to_string(operation, mode<0>() == AddressingMode::Quick);
	const bool flip_operands = (operation == Operation::MOVEMtoRl) || (operation == Operation::MOVEMtoRw);

	const std::string operand1 = operand_description(0 ^ int(flip_operands), opcode);
	const std::string operand2 = operand_description(1 ^ int(flip_operands), opcode);

	std::string result = instruction;
	if(!operand1.empty()) result += std::string(" ") + operand1;
	if(!operand2.empty()) result += std::string(", ") + operand2;

	const int extension_words = additional_extension_words();
	if(extension_words) result += std::string(" [+") + std::to_string(extension_words) + "]";

	return result;
}

const char *Preinstruction::operation_string() const {
	return _to_string(operation, mode<0>() == AddressingMode::Quick);
}
