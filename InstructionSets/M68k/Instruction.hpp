//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_68k_Instruction_hpp
#define InstructionSets_68k_Instruction_hpp

#include <cstdint>
#include "Model.hpp"

namespace InstructionSet {
namespace M68k {

enum class Operation: uint8_t {
	Undefined,

	NOP,

	ABCD,	SBCD,	NBCD,

	ADDb,	ADDw,	ADDl,
	ADDAw,	ADDAl,
	ADDXb,	ADDXw,	ADDXl,

	SUBb,	SUBw,	SUBl,
	SUBAw,	SUBAl,
	SUBXb,	SUBXw,	SUBXl,

	MOVEb,	MOVEw,	MOVEl,
	MOVEAw,	MOVEAl,
	LEA,	PEA,

	MOVEtoSR, MOVEfromSR,
	MOVEtoCCR,
	MOVEtoUSP, MOVEfromUSP,

	ORItoSR,	ORItoCCR,
	ANDItoSR,	ANDItoCCR,
	EORItoSR,	EORItoCCR,

	BTST,	BCLR,
	BCHG,	BSET,

	CMPb,	CMPw,	CMPl,
	CMPAw,	CMPAl,
	TSTb,	TSTw,	TSTl,

	JMP,
	JSR,	RTS,
	DBcc,
	Scc,

	Bccb,	Bccw,	Bccl,
	BSRb,	BSRw,	BSRl,

	CLRb, CLRw, CLRl,
	NEGXb, NEGXw, NEGXl,
	NEGb, NEGw, NEGl,

	ASLb, ASLw, ASLl, ASLm,
	ASRb, ASRw, ASRl, ASRm,
	LSLb, LSLw, LSLl, LSLm,
	LSRb, LSRw, LSRl, LSRm,
	ROLb, ROLw, ROLl, ROLm,
	RORb, RORw, RORl, RORm,
	ROXLb, ROXLw, ROXLl, ROXLm,
	ROXRb, ROXRw, ROXRl, ROXRm,

	MOVEMl, MOVEMw,
	MOVEPl, MOVEPw,

	ANDb,	ANDw,	ANDl,
	EORb,	EORw,	EORl,
	NOTb, 	NOTw, 	NOTl,
	ORb,	ORw,	ORl,

	MULU,	MULS,
	DIVU,	DIVS,

	RTE,	RTR,

	TRAP,	TRAPV,
	CHK,

	EXG,	SWAP,

	TAS,

	EXTbtow,	EXTwtol,

	LINKw,	UNLINK,

	STOP,	RESET,

	Max = RESET
};

template <Model model>
constexpr bool requires_supervisor(Operation op) {
	switch(op) {
		case Operation::MOVEfromSR:
			if constexpr (model == Model::M68000) {
				return false;
			}
			[[fallthrough]];
		case Operation::ORItoSR:	case Operation::ANDItoSR:
		case Operation::EORItoSR:	case Operation::RTE:
		case Operation::RESET:		case Operation::STOP:
		case Operation::MOVEtoUSP:	case Operation::MOVEfromUSP:
			return true;

		default:
			return false;
	}
}

enum class DataSize {
	Byte = 0,
	Word = 1,
	LongWord = 2,
};

/// Classifies operations by the size of their memory accesses, if any.
constexpr DataSize size(Operation operation) {
	switch(operation) {
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
		case Operation::NEGXb:	case Operation::NEGb:
		case Operation::ASLb:	case Operation::ASRb:
		case Operation::LSLb:	case Operation::LSRb:
		case Operation::ROLb:	case Operation::RORb:
		case Operation::ROXLb:	case Operation::ROXRb:
		case Operation::ANDb:	case Operation::EORb:
		case Operation::NOTb:	case Operation::ORb:
		case Operation::CHK:
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
		case Operation::DBcc:	case Operation::Scc:
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
		case Operation::MOVEMw:
		case Operation::MOVEPw:
		case Operation::ANDw:	case Operation::EORw:
		case Operation::NOTw:	case Operation::ORw:
		case Operation::DIVU:	case Operation::DIVS:
		case Operation::MULU:	case Operation::MULS:
		case Operation::EXTbtow:
		case Operation::LINKw:
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
		case Operation::MOVEMl:
		case Operation::MOVEPl:
		case Operation::ANDl:	case Operation::EORl:
		case Operation::NOTl:	case Operation::ORl:
		case Operation::EXTwtol:
		case Operation::UNLINK:
			return DataSize::LongWord;
	}
}

template <Operation op>
constexpr uint32_t quick(uint16_t instruction) {
	switch(op) {
		case Operation::Bccb:
		case Operation::BSRb:
		case Operation::MOVEl:	return uint32_t(int8_t(instruction));
		case Operation::TRAP:	return uint32_t(instruction & 15);
		default: {
			uint32_t value = (instruction >> 9) & 7;
			value |= (value - 1)&8;
			return value;
		}
	}
}

constexpr uint32_t quick(Operation op, uint16_t instruction) {
	switch(op) {
		case Operation::MOVEl:	return quick<Operation::MOVEl>(instruction);
		case Operation::Bccb:	return quick<Operation::Bccb>(instruction);
		case Operation::BSRb:	return quick<Operation::BSRb>(instruction);
		case Operation::TRAP:	return quick<Operation::TRAP>(instruction);

		default:
			// ADDw is arbitrary; anything other than those listed above will do.
			return quick<Operation::ADDw>(instruction);
	}
}

enum class Condition {
	True = 0x00,					False = 0x01,
	High = 0x02,					LowOrSame = 0x03,
	CarryClear = 0x04,				CarrySet = 0x05,
	NotEqual = 0x06,				Equal = 0x07,
	OverflowClear = 0x08,			OverflowSet = 0x09,
	Positive = 0x0a,				Negative = 0x0b,
	GreaterThanOrEqual = 0x0c,		LessThan = 0x0d,
	GreaterThan = 0x0e,				LessThanOrEqual = 0x0f,
};

/// Indicates the addressing mode applicable to an operand.
///
/// Implementation notes:
///
/// Those entries starting 0b00 or 0b01 are mapped as per the 68000's native encoding;
/// those starting 0b00 are those which are indicated directly by a mode field and those starting
/// 0b01 are those which are indicated by a register field given a mode of 0b111. The only minor
/// exception is AddressRegisterDirect, which exists on a 68000  but isn't specifiable by a
/// mode and register, it's contextual based on the instruction.
///
/// Those modes starting in 0b10 are the various extended addressing modes introduced as
/// of the 68020, which can be detected only after interpreting an extension word. At the
/// Preinstruction stage:
///
///	* AddressRegisterIndirectWithIndexBaseDisplacement, MemoryIndirectPostindexed
///		and MemoryIndirectPreindexed will have been partially decoded as
///		AddressRegisterIndirectWithIndex8bitDisplacement; and
///	* ProgramCounterIndirectWithIndexBaseDisplacement,
///		ProgramCounterMemoryIndirectPostindexed and
///		ProgramCounterMemoryIndirectPreindexed will have been partially decoded
///		as ProgramCounterIndirectWithIndex8bitDisplacement.
enum class AddressingMode: uint8_t {
	/// No adddressing mode; this operand doesn't exist.
	None												= 0b01'101,

	/// Dn
	DataRegisterDirect									= 0b00'000,

	/// An
	AddressRegisterDirect								= 0b00'001,
	/// (An)
	AddressRegisterIndirect								= 0b00'010,
	/// (An)+
	AddressRegisterIndirectWithPostincrement			= 0b00'011,
	/// -(An)
	AddressRegisterIndirectWithPredecrement				= 0b00'100,
	/// (d16, An)
	AddressRegisterIndirectWithDisplacement				= 0b00'101,
	/// (d8, An, Xn)
	AddressRegisterIndirectWithIndex8bitDisplacement	= 0b00'110,
	/// (bd, An, Xn)
	AddressRegisterIndirectWithIndexBaseDisplacement	= 0b10'000,

	/// ([bd, An, Xn], od)
	MemoryIndirectPostindexed							= 0b10'001,
	/// ([bd, An], Xn, od)
	MemoryIndirectPreindexed							= 0b10'010,

	/// (d16, PC)
	ProgramCounterIndirectWithDisplacement				= 0b01'010,
	/// (d8, PC, Xn)
	ProgramCounterIndirectWithIndex8bitDisplacement		= 0b01'011,
	/// (bd, PC, Xn)
	ProgramCounterIndirectWithIndexBaseDisplacement		= 0b10'011,
	/// ([bd, PC, Xn], od)
	ProgramCounterMemoryIndirectPostindexed				= 0b10'100,
	/// ([bc, PC], Xn, od)
	ProgramCounterMemoryIndirectPreindexed				= 0b10'101,

	/// (xxx).W
	AbsoluteShort										= 0b01'000,
	/// (xxx).L
	AbsoluteLong										= 0b01'001,

	/// #
	ImmediateData										= 0b01'100,

	/// .q; value is embedded in the opcode.
	Quick												= 0b01'110,
};

/*!
	A preinstruction is as much of an instruction as can be decoded with
	only the first instruction word — i.e. an operation, and:

	* on the 68000 and 68010, the complete addressing modes;
	* on subsequent, a decent proportion of the addressing mode. See
		the notes on @c AddressingMode for potential aliasing.
*/
class Preinstruction {
	public:
		Operation operation = Operation::Undefined;

		// Instructions come with 0, 1 or 2 operands;
		// the getters below act to provide a list of operands
		// that is terminated by an AddressingMode::None.
		//
		// For two-operand instructions, argument 0 is a source
		// and argument 1 is a destination.
		//
		// For one-operand instructions, only argument 0 will
		// be provided, and will be a source and/or destination as
		// per the semantics of the operation.

		template <int index> AddressingMode mode() const {
			if constexpr (index > 1) {
				return AddressingMode::None;
			}
			return AddressingMode(operands_[index] & 0x1f);
		}
		template <int index> int reg() const {
			if constexpr (index > 1) {
				return 0;
			}
			return operands_[index] >> 5;
		}

		bool requires_supervisor() {
			return flags_ & 0x80;
		}
		DataSize size() {
			return DataSize(flags_ & 0x03);
		}
		Condition condition() {
			return Condition((flags_ >> 2) & 0x0f);
		}

	private:
		uint8_t operands_[2] = { uint8_t(AddressingMode::None), uint8_t(AddressingMode::None)};
		uint8_t flags_ = 0;

	public:
		Preinstruction(
			Operation operation,
			AddressingMode op1_mode,	int op1_reg,
			AddressingMode op2_mode,	int op2_reg,
			bool is_supervisor,
			DataSize size,
			Condition condition) : operation(operation)
		{
			operands_[0] = uint8_t(op1_mode) | uint8_t(op1_reg << 5);
			operands_[1] = uint8_t(op2_mode) | uint8_t(op2_reg << 5);
			flags_ = uint8_t(
				(is_supervisor ? 0x80 : 0x00) |
				(int(condition) << 2) |
				int(size)
			);
		}

		Preinstruction() {}
};

}
}

#endif /* InstructionSets_68k_Instruction_hpp */
