//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_68k_Instruction_hpp
#define InstructionSets_68k_Instruction_hpp

#include "Model.hpp"

#include <cassert>
#include <cstdint>

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

	MOVEMtoRl, MOVEMtoRw,
	MOVEMtoMl, MOVEMtoMw,

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
///
/// For any operations that don't fit the neat model of reading one or two operands,
/// then writing zero or one, the size determines the data size of the operands only,
/// not any other accesses.
constexpr DataSize operand_size(Operation operation) {
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
		case Operation::Scc:
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
	}
}

template <Operation t_op = Operation::Undefined>
constexpr uint32_t quick(uint16_t instruction, Operation r_op = Operation::Undefined) {
	switch((t_op != Operation::Undefined) ? t_op : r_op) {
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
		//	(which means that source and destination will appear as their effective addresses)
		//
		case Operation::PEA:
		case Operation::JMP:		case Operation::JSR:
		case Operation::MOVEPw:		case Operation::MOVEPl:
		case Operation::MOVEMtoMw:	case Operation::MOVEMtoMl:
		case Operation::MOVEMtoRw:	case Operation::MOVEMtoRl:
		case Operation::TAS:
		case Operation::RTR:		case Operation::RTS:		case Operation::RTE:
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
			return FetchOp1;

		//
		//	Single-operand write.
		//
		case Operation::MOVEfromSR:	case Operation::MOVEfromUSP:
		case Operation::Scc:
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
			return FetchOp1 | StoreOp1;

		//
		//	CLR, which is model-dependent.
		//
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
		case Operation::CHK:
		case Operation::BTST:
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
		case Operation::DIVU:	case Operation::DIVS:
		case Operation::MULU:	case Operation::MULS:
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
		case Operation::LINKw:
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
	/// (bd, An, Xn)		[68020+]
	AddressRegisterIndirectWithIndexBaseDisplacement	= 0b10'000,

	/// ([bd, An, Xn], od)	[68020+]
	MemoryIndirectPostindexed							= 0b10'001,
	/// ([bd, An], Xn, od)	[68020+]
	MemoryIndirectPreindexed							= 0b10'010,

	/// (d16, PC)
	ProgramCounterIndirectWithDisplacement				= 0b01'010,
	/// (d8, PC, Xn)
	ProgramCounterIndirectWithIndex8bitDisplacement		= 0b01'011,
	/// (bd, PC, Xn)		[68020+]
	ProgramCounterIndirectWithIndexBaseDisplacement		= 0b10'011,
	/// ([bd, PC, Xn], od)	[68020+]
	ProgramCounterMemoryIndirectPostindexed				= 0b10'100,
	/// ([bc, PC], Xn, od)	[68020+]
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
		//
		// The versions templated on index do a range check;
		// if using the runtime versions then results for indices
		// other than 0 and 1 are undefined.

		AddressingMode mode(int index) const {
			return AddressingMode(operands_[index] & 0x1f);
		}
		template <int index> AddressingMode mode() const {
			if constexpr (index > 1) {
				return AddressingMode::None;
			}
			return mode(index);
		}
		int reg(int index) const {
			return operands_[index] >> 5;
		}
		template <int index> int reg() const {
			if constexpr (index > 1) {
				return 0;
			}
			return reg(index);
		}

		bool requires_supervisor() const {
			return flags_ & 0x80;
		}
		DataSize operand_size() const {
			return DataSize(flags_ & 0x03);
		}
		Condition condition() const {
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
