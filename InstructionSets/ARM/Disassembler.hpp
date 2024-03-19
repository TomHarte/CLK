//
//  Disassembler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "OperationMapper.hpp"

#include <string>
#include <sstream>

namespace InstructionSet::ARM {

struct Operand {
	enum class Type {
		Immediate, Register, RegisterList, None
	} type = Type::None;
	uint32_t value = 0;

	// TODO: encode shifting

	operator std::string() const {
		return "";
	}
};

struct Instruction {
	Condition condition;
	enum class Operation {
		AND,	EOR,	SUB,	RSB,
		ADD,	ADC,	SBC,	RSC,
		TST,	TEQ,	CMP,	CMN,
		ORR,	MOV,	BIC,	MVN,

		B, BL,

		SWI,

		Undefined,
	} operation = Operation::Undefined;

	Operand destination, operand1, operand2;

	std::string to_string(uint32_t address) const {
		std::ostringstream result;

		// Treat all nevers as nops.
		if(condition == Condition::NV) {
			return "nop";
		}

		// Print operation.
		switch(operation) {
			case Operation::Undefined:	return "undefined";
			case Operation::SWI:		return "swi";

			case Operation::B:		result << "b";		break;
			case Operation::BL:		result << "bl";		break;

			case Operation::AND:	result << "and";	break;
			case Operation::EOR:	result << "eor";	break;
			case Operation::SUB:	result << "sub";	break;
			case Operation::RSB:	result << "rsb";	break;
			case Operation::ADD:	result << "add";	break;
			case Operation::ADC:	result << "adc";	break;
			case Operation::SBC:	result << "sbc";	break;
			case Operation::RSC:	result << "rsc";	break;
			case Operation::TST:	result << "tst";	break;
			case Operation::TEQ:	result << "teq";	break;
			case Operation::CMP:	result << "cmp";	break;
			case Operation::CMN:	result << "cmn";	break;
			case Operation::ORR:	result << "orr";	break;
			case Operation::MOV:	result << "mov";	break;
			case Operation::BIC:	result << "bic";	break;
			case Operation::MVN:	result << "mvn";	break;
		}

		// If this is a branch, append the target and complete.
		if(operation == Operation::B || operation == Operation::BL) {
			result << " 0x" << std::hex << ((address + 8 + operand1.value) & 0x3fffffc);
			return result.str();
		}

		return result.str();
	}
};

template <Model model>
struct Disassembler {
	Instruction last() {
		return instruction_;
	}

	bool should_schedule(Condition condition) {
		instruction_ = Instruction();
		instruction_.condition = condition;
		return true;
	}

	template <Flags> void perform(DataProcessing) {}
	template <Flags> void perform(Multiply) {}
	template <Flags> void perform(SingleDataTransfer) {}
	template <Flags> void perform(BlockDataTransfer) {}
	template <Flags f> void perform(Branch fields) {
		constexpr BranchFlags flags(f);
		instruction_.operation =
			(flags.operation() == BranchFlags::Operation::BL) ?
				Instruction::Operation::BL : Instruction::Operation::B;
		instruction_.operand1.type = Operand::Type::Immediate;
		instruction_.operand1.value = fields.offset();
	}
	template <Flags> void perform(CoprocessorRegisterTransfer) {}
	template <Flags> void perform(CoprocessorDataOperation) {}
	template <Flags> void perform(CoprocessorDataTransfer) {}

	void software_interrupt() {
		instruction_.operation = Instruction::Operation::SWI;
	}
	void unknown() {
		instruction_.operation = Instruction::Operation::Undefined;
	}

private:
	Instruction instruction_;

};

}
