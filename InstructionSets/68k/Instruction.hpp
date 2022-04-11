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

namespace InstructionSet {
namespace M68k {

enum class Operation: uint8_t {
	Undefined,

	ABCD,	SBCD,	NBCD,

	ADDb,	ADDw,	ADDl,
	ADDQb,	ADDQw,	ADDQl,
	ADDAw,	ADDAl,
	ADDQAw,	ADDQAl,
	ADDXb,	ADDXw,	ADDXl,

	SUBb,	SUBw,	SUBl,
	SUBQb,	SUBQw,	SUBQl,
	SUBAw,	SUBAl,
	SUBQAw,	SUBQAl,
	SUBXb,	SUBXw,	SUBXl,

	MOVEb,	MOVEw,	MOVEl,	MOVEq,
	MOVEAw,	MOVEAl,
	PEA,

	MOVEtoSR, MOVEfromSR,
	MOVEtoCCR,

	ORItoSR,	ORItoCCR,
	ANDItoSR,	ANDItoCCR,
	EORItoSR,	EORItoCCR,

	BTSTb,	BTSTl,
	BCLRl,	BCLRb,
	CMPb,	CMPw,	CMPl,
	CMPAw,
	TSTb,	TSTw,	TSTl,

	JMP,	RTS,
	BRA,	Bcc,
	DBcc,
	Scc,

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

	MOVEPtoRl, MOVEPtoRw,
	MOVEPtoMl, MOVEPtoMw,

	ANDb,	ANDw,	ANDl,
	EORb,	EORw,	EORl,
	NOTb, 	NOTw, 	NOTl,
	ORb,	ORw,	ORl,

	MULU,	MULS,
	DIVU,	DIVS,

	RTE_RTR,

	TRAP,	TRAPV,
	CHK,

	EXG,	SWAP,

	BCHGl,	BCHGb,
	BSETl,	BSETb,

	TAS,

	EXTbtow,	EXTwtol,

	LINK,	UNLINK,

	STOP,
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
	None												= 0b11'111,

	/// Dn
	DataRegisterDirect									= 0b00'000,

	/// An
	AddressRegisterDirect								= 0b11'000,
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

		// First operand.
		AddressingMode source_mode() {
			return AddressingMode(source_ & 0x1f);
		}
		int source() {
			return source_ >> 5;
		}

		// Second operand.
		AddressingMode destination_mode() {
			return AddressingMode(destination_ & 0x1f);
		}
		int destination() {
			return destination_ >> 5;
		}

	private:
		uint8_t source_ = 0;
		uint8_t destination_ = 0;

	public:
		Preinstruction(
			Operation operation,
			AddressingMode source_mode,
			int source,
			AddressingMode destination_mode,
			int destination) : operation(operation) {
			source_ = uint8_t(source_mode) | uint8_t(source << 5);
			destination_ = uint8_t(destination_mode) | uint8_t(destination << 5);
		}

		Preinstruction() {}
};

}
}

#endif /* InstructionSets_68k_Instruction_hpp */
