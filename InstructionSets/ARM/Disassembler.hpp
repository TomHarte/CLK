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
		switch(type) {
			default:	return "";
			case Type::Register:	return std::string("r") + std::to_string(value);
		}
	}
};

struct Instruction {
	Condition condition = Condition::AL;
	enum class Operation {
		AND,	EOR,	SUB,	RSB,
		ADD,	ADC,	SBC,	RSC,
		TST,	TEQ,	CMP,	CMN,
		ORR,	MOV,	BIC,	MVN,

		LDR,	STR,

		B, BL,

		SWI,

		Undefined,
	} operation = Operation::Undefined;

	Operand destination, operand1, operand2;
	bool sets_flags = false;

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

			case Operation::LDR:	result << "ldr";	break;
			case Operation::STR:	result << "str";	break;
		}

		// Append the sets-flags modifier if applicable.
		if(sets_flags) result << 's';

		// Possibly a condition code.
		switch(condition) {
			case Condition::EQ:	result << "eq";	break;
			case Condition::NE:	result << "ne";	break;
			case Condition::CS:	result << "cs";	break;
			case Condition::CC:	result << "cc";	break;
			case Condition::MI:	result << "mi";	break;
			case Condition::PL:	result << "pl";	break;
			case Condition::VS:	result << "vs";	break;
			case Condition::VC:	result << "vc";	break;
			case Condition::HI:	result << "hi";	break;
			case Condition::LS:	result << "ls";	break;
			case Condition::GE:	result << "ge";	break;
			case Condition::LT:	result << "lt";	break;
			case Condition::GT:	result << "gt";	break;
			case Condition::LE:	result << "le";	break;
			default: break;
		}

		// If this is a branch, append the target.
		if(operation == Operation::B || operation == Operation::BL) {
			result << " 0x" << std::hex << ((address + 8 + operand1.value) & 0x3fffffc);
		}

		if(operation == Operation::LDR || operation == Operation::STR) {
			result << ' ' << static_cast<std::string>(destination);
			result << ", [" << static_cast<std::string>(operand1) << "]";
			// TODO: learn how ARM shifts/etc are normally represented.
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

	template <Flags f> void perform(DataProcessing fields) {
		constexpr DataProcessingFlags flags(f);

		//
		instruction_.operand1.type = Operand::Type::Register;
		instruction_.operand1.value = fields.operand1();

		instruction_.sets_flags = flags.set_condition_codes();
	}

	template <Flags> void perform(Multiply) {}
	template <Flags f> void perform(SingleDataTransfer fields) {
		constexpr SingleDataTransferFlags flags(f);
		instruction_.operation =
			(flags.operation() == SingleDataTransferFlags::Operation::STR) ?
				Instruction::Operation::STR : Instruction::Operation::LDR;

		instruction_.destination.type = Operand::Type::Register;
		instruction_.destination.value = fields.destination();

		instruction_.operand1.type = Operand::Type::Register;
		instruction_.operand1.value = fields.base();
	}
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
