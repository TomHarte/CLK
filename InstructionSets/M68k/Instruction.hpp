//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#pragma once

#include "Model.hpp"

#include <cassert>
#include <cstdint>
#include <string>

namespace InstructionSet::M68k {

enum class Operation: uint8_t {
	Undefined,

	//
	// 68000 operations.
	//

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

	Bccb,	Bccw,
	BSRb,	BSRw,

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
	NOTb,	NOTw,	NOTl,
	ORb,	ORw,	ORl,

	MULUw,	MULSw,
	DIVUw,	DIVSw,

	RTE,	RTR,

	TRAP,	TRAPV,
	CHKw,

	EXG,	SWAP,

	TAS,

	EXTbtow,	EXTwtol,

	LINKw,	UNLINK,

	STOP,	RESET,

	//
	// 68010 additions.
	//

	MOVEfromCCR,
	MOVEtoC,	MOVEfromC,
	MOVESb,	MOVESw,	MOVESl,
	BKPT,	RTD,

	//
	// 68020 additions.
	//

	TRAPcc,

	CALLM,	RTM,

	BFCHG,	BFCLR,
	BFEXTS,	BFEXTU,
	BFFFO,	BFINS,
	BFSET,	BFTST,

	PACK,	UNPK,

	CASb,	CASw,	CASl,
	CAS2w,	CAS2l,

	// CHK2 and CMP2 are distinguished by their extension word;
	// since this code deals in Preinstructions, i.e. as much
	// as can be derived from the instruction word alone, in addition
	// to the full things, the following enums result.
	CHKorCMP2b,	CHKorCMP2w,	CHKorCMP2l,

	// DIVS.l, DIVSL.l, DIVU.l and DIVUL.l are all distinguishable
	// only by the extension word.
	DIVSorDIVUl,

	// MULS.l, MULSL.l, MULU.l and MULUL.l are all distinguishable
	// only by the extension word.
	MULSorMULUl,

	Bccl,	BSRl,
	LINKl,	CHKl,

	EXTbtol,

	// Coprocessor instructions are omitted for now, until I can
	// determine by what mechanism the number of
	// "OPTIONAL COPROCESSOR-DEFINED EXTENSION WORDS" is determined.
//	cpBcc,	cpDBcc,		cpGEN,
//	cpScc,	cpTRAPcc,	cpRESTORE,
//	cpSAVE,

	//
	// 68030 additions.
	//

	PFLUSH,	PFLUSHA,
	PLOADR, PLOADW,
	PMOVE, PMOVEFD,
	PTESTR, PTESTW,

	//
	// 68040 additions.
	//

	// TODO: the big addition of the 68040 is incorporation of the FPU; should I make decoding of those instructions
	// dependent upon a 68040 being selected, or should I offer a separate decoder in order to support systems with
	// a coprocessor?

	//
	// Introspection.
	//
	Max68000 = RESET,
	Max68010 = RTD,
	Max68020 = EXTbtol,
	Max68030 = PTESTW,
	Max68040 = PTESTW,
};

// Provide per-model max entries in Operation.
template <Model> struct OperationMax {};
template <> struct OperationMax<Model::M68000> {
	static constexpr Operation value = Operation::Max68000;
};
template <> struct OperationMax<Model::M68010> {
	static constexpr Operation value = Operation::Max68010;
};
template <> struct OperationMax<Model::M68020> {
	static constexpr Operation value = Operation::Max68020;
};
template <> struct OperationMax<Model::M68030> {
	static constexpr Operation value = Operation::Max68030;
};
template <> struct OperationMax<Model::M68040> {
	static constexpr Operation value = Operation::Max68040;
};

const char *to_string(Operation op);

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
		case Operation::MOVEtoC:	case Operation::MOVEfromC:
		case Operation::MOVEtoSR:
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
template <Operation t_operation = Operation::Undefined>
constexpr DataSize operand_size(Operation operation = Operation::Undefined);

template <Operation t_op = Operation::Undefined>
constexpr uint32_t quick(uint16_t instruction, Operation r_op = Operation::Undefined) {
	switch((t_op != Operation::Undefined) ? t_op : r_op) {
		case Operation::Bccb:
		case Operation::BSRb:
		case Operation::MOVEl:	return uint32_t(int8_t(instruction));
		case Operation::TRAP:	return uint32_t(instruction & 15);
		case Operation::BKPT:	return uint32_t(instruction & 7);
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
	operands are accessed via standard fetch and store cycles; the bitfield is composted of
	[Fetch/Store]Op[1/2] as defined above.

	Unusual bus sequences, such as TAS or MOVEM, are not described here.
*/
template <Model model, Operation t_operation = Operation::Undefined>
constexpr uint8_t operand_flags(Operation r_operation = Operation::Undefined);

/// Lists the various condition codes used by the 680x0.
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
/// exception is AddressRegisterDirect, which exists on a 68000 but isn't specifiable by a
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

	/// An additional word of data. Differs from ImmediateData by being
	/// a fixed size, rather than the @c operand_size of the operation.
	ExtensionWord										= 0b01'111,

	/// .q; value is embedded in the opcode.
	Quick												= 0b01'110,
};
/// Guaranteed to be 1+[largest value used by AddressingMode].
static constexpr int AddressingModeCount = 0b10'110;

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
			return AddressingMode(operands_[index] >> 3);
		}
		template <int index> AddressingMode mode() const {
			if constexpr (index > 1) {
				return AddressingMode::None;
			}
			return mode(index);
		}
		int reg(int index) const {
			return operands_[index] & 7;
		}
		template <int index> int reg() const {
			if constexpr (index > 1) {
				return 0;
			}
			return reg(index);
		}

		/// @returns 0–7 to indicate data registers 0 to 7, or 8–15 to indicate address registers 0 to 7 respectively.
		/// Provides undefined results if the addressing mode is not either @c DataRegisterDirect or
		/// @c AddressRegisterDirect.
		int lreg(int index) const {
			return operands_[index] & 0xf;
		}

		/// @returns @c true if this instruction requires supervisor privileges; @c false otherwise.
		bool requires_supervisor() const {
			return flags_ & Flags::IsSupervisor;
		}
		/// @returns @c true if this instruction will require further fetching than can be encoded in a
		/// @c Preinstruction. In practice this means it is one of a very small quantity of 68020+
		/// instructions; those that can rationalise extension words into one of the two operands will
		/// do so. Use the free function @c extension_words(instruction.operation) to
		/// look up the number of additional words required.
		///
		/// (specifically affected, at least: PACK, UNPK, CAS, CAS2)
		bool requires_further_extension() const {
			return flags_ & Flags::RequiresFurtherExtension;
		}
		/// @returns The number of additional extension words required, beyond those encoded as operands.
		int additional_extension_words() const {
			return flags_ & Flags::RequiresFurtherExtension ? (flags_ & Flags::ConditionMask) >> Flags::ConditionShift : 0;
		}
		/// @returns The @c DataSize used for operands of this instruction, i.e. byte, word or longword.
		DataSize operand_size() const {
			return DataSize((flags_ & Flags::SizeMask) >> Flags::SizeShift);
		}
		/// @returns The condition code evaluated by this instruction if applicable. If this instruction is not
		/// conditional, the result is undefined.
		Condition condition() const {
			return Condition((flags_ & Flags::ConditionMask) >> Flags::ConditionShift);
		}

	private:
		uint8_t operands_[2] = { uint8_t(AddressingMode::None), uint8_t(AddressingMode::None)};
		uint8_t flags_ = 0;

		std::string operand_description(int index, int opcode) const;

	public:
		Preinstruction(
			Operation operation,
			AddressingMode op1_mode,	int op1_reg,
			AddressingMode op2_mode,	int op2_reg,
			bool is_supervisor,
			int extension_words,
			DataSize size,
			Condition condition) : operation(operation)
		{
			operands_[0] = uint8_t((uint8_t(op1_mode) << 3) | op1_reg);
			operands_[1] = uint8_t((uint8_t(op2_mode) << 3) | op2_reg);
			flags_ = uint8_t(
				(is_supervisor ? Flags::IsSupervisor : 0x00) |
				(extension_words ? Flags::RequiresFurtherExtension : 0x00) |
				(int(condition) << Flags::ConditionShift) |
				(extension_words << Flags::ConditionShift) |
				(int(size) << Flags::SizeShift)
			);
		}

		struct Flags {
			static constexpr uint8_t IsSupervisor				= 0b1000'0000;
			static constexpr uint8_t RequiresFurtherExtension	= 0b0100'0000;
			static constexpr uint8_t ConditionMask				= 0b0011'1100;
			static constexpr uint8_t SizeMask					= 0b0000'0011;

			static constexpr int IsSupervisorShift				= 7;
			static constexpr int RequiresFurtherExtensionShift	= 6;
			static constexpr int ConditionShift					= 2;
			static constexpr int SizeShift						= 0;
		};

		Preinstruction() {}

		/// Produces a string description of this instruction; if @c opcode
		/// is supplied then any quick fields in this instruction will be decoded;
		/// otherwise they'll be printed as just 'Q'.
		std::string to_string(int opcode = -1) const;

		/// Produces a slightly-more-idiomatic version of the operation name than
		/// a direct to_string(instruction.operation) would, given that this decoder
		/// sometimes aliases operations, disambiguating based on addressing mode
		/// (e.g. MOVEQ is MOVE.l with the Q addressing mode).
		const char *operation_string() const;
};

}

#include "Implementation/InstructionOperandSize.hpp"
#include "Implementation/InstructionOperandFlags.hpp"
