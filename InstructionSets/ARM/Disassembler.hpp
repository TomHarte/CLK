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

/// Holds a single ARM operand, whether a source/destination or immediate value, potentially including a shift.
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
			case Type::RegisterList: {
				std::stringstream stream;
				stream << '[';
				bool first = true;
				for(int c = 0; c < 16; c++) {
					if(value & (1 << c)) {
						if(!first) stream << ", ";
						first = false;

						stream << 'r' << c;
					}
				}
				stream << ']';

				return stream.str();
			}
		}
	}
};

/// Describes a single ARM instruction, suboptimally but such that all relevant detail has been extracted
/// by the OperationMapper and is now easy to inspect or to turn into a string.
struct Instruction {
	Condition condition = Condition::AL;
	enum class Operation {
		AND,	EOR,	SUB,	RSB,
		ADD,	ADC,	SBC,	RSC,
		TST,	TEQ,	CMP,	CMN,
		ORR,	MOV,	BIC,	MVN,

		LDR,	STR,
		LDM,	STM,

		B, BL,

		SWI,

		MRC, MCR,

		Undefined,
	} operation = Operation::Undefined;

	Operand destination, operand1, operand2;
	bool sets_flags = false;
	bool is_byte = false;

	std::string to_string(const uint32_t address) const {
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
			case Operation::LDM:	result << "ldm";	break;
			case Operation::STM:	result << "stm";	break;

			case Operation::MRC:	result << "mrc";	break;
			case Operation::MCR:	result << "mcr";	break;
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

		if(
			operation == Operation::LDR || operation == Operation::STR ||
			operation == Operation::LDM || operation == Operation::STM
		) {
			if(is_byte) result << 'b';
			result << ' ' << static_cast<std::string>(destination);
			result << ", [" << static_cast<std::string>(operand1) << "]";
			// TODO: learn how ARM shifts/etc are normally presented.
		}

		return result.str();
	}
};

/// A target for @c dispatch that merely captures a description of the decoded instruction, being
/// able to vend it later via @c last().
template <Model model>
struct Disassembler {
	Instruction last() const {
		return instruction_;
	}

	bool should_schedule(const Condition condition) {
		instruction_ = Instruction();
		instruction_.condition = condition;
		return true;
	}

	template <Flags f> void perform(const DataProcessing fields) {
		static constexpr DataProcessingFlags flags(f);

		instruction_.operand1.type = Operand::Type::Register;
		instruction_.operand1.value = fields.operand1();

		instruction_.destination.type = Operand::Type::Register;
		instruction_.destination.value = fields.destination();

		if(flags.operand2_is_immediate()) {
			instruction_.operand2.type = Operand::Type::Immediate;
//			instruction_.operand2.value = fields.immediate(), fields.rotate();
			// TODO: decode immediate.

		} else {
			instruction_.operand2.type = Operand::Type::Register;
			instruction_.operand2.value = fields.operand2();
			// TODO: capture shift_type(), etc.
		}

		instruction_.sets_flags = flags.set_condition_codes();

		switch(flags.operation()) {
			case DataProcessingOperation::AND:	instruction_.operation = Instruction::Operation::AND;	break;
			case DataProcessingOperation::EOR:	instruction_.operation = Instruction::Operation::EOR;	break;
			case DataProcessingOperation::ORR:	instruction_.operation = Instruction::Operation::ORR;	break;
			case DataProcessingOperation::BIC:	instruction_.operation = Instruction::Operation::BIC;	break;
			case DataProcessingOperation::MOV:	instruction_.operation = Instruction::Operation::MOV;	break;
			case DataProcessingOperation::MVN:	instruction_.operation = Instruction::Operation::MVN;	break;
			case DataProcessingOperation::TST:	instruction_.operation = Instruction::Operation::TST;	break;
			case DataProcessingOperation::TEQ:	instruction_.operation = Instruction::Operation::TEQ;	break;
			case DataProcessingOperation::ADD:	instruction_.operation = Instruction::Operation::ADD;	break;
			case DataProcessingOperation::ADC:	instruction_.operation = Instruction::Operation::ADC;	break;
			case DataProcessingOperation::CMN:	instruction_.operation = Instruction::Operation::CMN;	break;
			case DataProcessingOperation::SUB:	instruction_.operation = Instruction::Operation::SUB;	break;
			case DataProcessingOperation::SBC:	instruction_.operation = Instruction::Operation::SBC;	break;
			case DataProcessingOperation::CMP:	instruction_.operation = Instruction::Operation::CMP;	break;
			case DataProcessingOperation::RSB:	instruction_.operation = Instruction::Operation::RSB;	break;
			case DataProcessingOperation::RSC:	instruction_.operation = Instruction::Operation::RSC;	break;
		}
	}

	template <Flags> void perform(Multiply) {}
	template <Flags f> void perform(const SingleDataTransfer fields) {
		static constexpr SingleDataTransferFlags flags(f);
		instruction_.operation =
			(flags.operation() == SingleDataTransferFlags::Operation::STR) ?
				Instruction::Operation::STR : Instruction::Operation::LDR;

		instruction_.destination.type = Operand::Type::Register;
		instruction_.destination.value = fields.destination();

		instruction_.operand1.type = Operand::Type::Register;
		instruction_.operand1.value = fields.base();
	}
	template <Flags f> void perform(const BlockDataTransfer fields) {
		static constexpr BlockDataTransferFlags flags(f);
		instruction_.operation =
			(flags.operation() == BlockDataTransferFlags::Operation::STM) ?
				Instruction::Operation::STM : Instruction::Operation::LDM;

		instruction_.destination.type = Operand::Type::Register;
		instruction_.destination.value = fields.base();

		instruction_.operand1.type = Operand::Type::RegisterList;
		instruction_.operand1.value = fields.register_list();
	}
	template <Flags f> void perform(const Branch fields) {
		static constexpr BranchFlags flags(f);
		instruction_.operation =
			(flags.operation() == BranchFlags::Operation::BL) ?
				Instruction::Operation::BL : Instruction::Operation::B;
		instruction_.operand1.type = Operand::Type::Immediate;
		instruction_.operand1.value = fields.offset();
	}
	template <Flags f> void perform(CoprocessorRegisterTransfer) {
		static constexpr CoprocessorRegisterTransferFlags flags(f);
		instruction_.operation =
			(flags.operation() == CoprocessorRegisterTransferFlags::Operation::MRC) ?
				Instruction::Operation::MRC : Instruction::Operation::MCR;
	}
	template <Flags> void perform(CoprocessorDataOperation) {}
	template <Flags> void perform(CoprocessorDataTransfer) {}

	void software_interrupt(SoftwareInterrupt) {
		instruction_.operation = Instruction::Operation::SWI;
	}
	void unknown() {
		instruction_.operation = Instruction::Operation::Undefined;
	}

private:
	Instruction instruction_;
};

}
