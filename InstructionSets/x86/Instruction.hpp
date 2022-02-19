//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_x86_Instruction_h
#define InstructionSets_x86_Instruction_h

#include <cstdint>

namespace InstructionSet {
namespace x86 {

/*
	Operations are documented below to establish expectations as to which
	instruction fields will be meaningful for each; this is a work-in-progress
	and may currently contain errors in the opcode descriptions — especially
	where implicit register dependencies are afoot.
*/
enum class Operation: uint8_t {
	Invalid,

	//
	// 8086 instructions.
	//

	/// ASCII adjust after addition; source will be AL and destination will be AX.
	AAA,
	/// ASCII adjust before division; destination will be AX and source will be a multiplier.
	AAD,
	/// ASCII adjust after multiplication; destination will be AX and source will be a divider.
	AAM,
	/// ASCII adjust after subtraction; source will be AL and destination will be AX.
	AAS,
	/// Decimal adjust after addition; source and destination will be AL.
	DAA,
	/// Decimal adjust after subtraction; source and destination will be AL.
	DAS,

	/// Convert byte into word; source will be AL, destination will be AH.
	CBW,
	/// Convert word to double word; source will be AX and destination will be DX.
	CWD,

	/// Escape, for a coprocessor; perform the bus cycles necessary to read the source and destination and perform a NOP.
	ESC,

	/// Stops the processor until the next interrupt is fired.
	HLT,
	/// Waits until the WAIT input is asserted; if an interrupt occurs then it is serviced but returns to the WAIT.
	WAIT,

	/// Add with carry; source, destination, operand and displacement will be populated appropriately.
	ADC,
	/// Add; source, destination, operand and displacement will be populated appropriately.
	ADD,
	/// Subtract with borrow; source, destination, operand and displacement will be populated appropriately.
	SBB,
	/// Subtract; source, destination, operand and displacement will be populated appropriately.
	SUB,
	/// Unsigned multiply; multiplies the source value by AX or AL, storing the result in DX:AX or AX.
	MUL,
	/// Signed multiply; multiplies the source value by AX or AL, storing the result in DX:AX or AX.
	IMUL,
	/// Unsigned divide; divide the source value by AX or AL, storing the quotient in AL and the remainder in AH.
	DIV,
	/// Signed divide; divide the source value by AX or AL, storing the quotient in AL and the remainder in AH.
	IDIV,

	/// Increment; source, destination, operand and displacement will be populated appropriately.
	INC,
	/// Decrement; source, destination, operand and displacement will be populated appropriately.
	DEC,

	/// Reads from the port specified by source to the destination.
	IN,
	/// Writes from the port specified by destination from the source.
	OUT,

	// Various jumps; see the displacement to calculate targets.
	JO,	JNO,	JB, JNB,	JE, JNE,	JBE, JNBE,
	JS, JNS,	JP, JNP,	JL, JNL,	JLE, JNLE,

	/// Far call; see the segment() and offset() fields.
	CALLF,
	/// Displacement call; followed by a 16-bit operand providing a call offset.
	CALLD,
	/// Near call.
	CALLN,
	/// Return from interrupt.
	IRET,
	/// Near return; if source is not ::None then it will be an ::Immediate indicating how many additional bytes to remove from the stack.
	RETF,
	/// Far return; if source is not ::None then it will be an ::Immediate indicating how many additional bytes to remove from the stack.
	RETN,
	/// Near jump; if an operand is not ::None then it gives an absolute destination; otherwise see the displacement.
	JMPN,
	/// Far jump to the indicated segment and offset.
	JMPF,
	/// Relative jump performed only if CX = 0; see the displacement.
	JPCX,
	/// Generates a software interrupt of the level stated in the operand.
	INT,
	/// Generates a software interrupt of level 3.
	INT3,
	/// Generates a software interrupt of level 4 if overflow is set.
	INTO,

	/// Load status flags to AH.
	LAHF,
	/// Load status flags from AH.
	SAHF,
	/// Load a segment and offset from the source into DS and the destination.
	LDS,
	/// Load a segment and offset from the source into ES and the destination.
	LES,
	/// Computes the effective address of the source and loads it into the destination.
	LEA,

	/// Compare [bytes or words, per operation size]; source and destination implied to be DS:[SI] and ES:[DI].
	CMPS,
	/// Load string; reads from DS:SI into AL or AX, subject to segment override.
	LODS,
	/// Move string; moves a byte or word from DS:SI to ES:DI. If a segment override is provided, it overrides the the source.
	MOVS,
	/// Scan string; reads a byte or word from DS:SI and compares it to AL or AX.
	SCAS,
	/// Store string; store AL or AX to ES:DI.
	STOS,

	// Perform a possibly-conditional loop, decrementing CX. See the displacement.
	LOOP, LOOPE, LOOPNE,

	/// Loads the destination with the source.
	MOV,
	/// Negatives; source and destination point to the same thing, to negative.
	NEG,
	/// Logical NOT;  source and destination point to the same thing, to negative.
	NOT,
	/// Logical AND; source, destination, operand and displacement will be populated appropriately.
	AND,
	/// Logical OR of source onto destination.
	OR,
	/// Logical XOR of source onto destination.
	XOR,
	/// NOP; no further fields.
	NOP,
	/// POP from the stack to destination.
	POP,
	/// POP from the stack to the flags register.
	POPF,
	/// PUSH the source to the stack.
	PUSH,
	/// PUSH the flags register to the stack.
	PUSHF,
	/// Rotate the destination left through carry the number of bits indicated by source.
	RCL,
	/// Rotate the destination right through carry the number of bits indicated by source.
	RCR,
	/// Rotate the destination left the number of bits indicated by source.
	ROL,
	/// Rotate the destination right the number of bits indicated by source.
	ROR,
	/// Arithmetic shift left the destination by the number of bits indicated by source.
	SAL,
	/// Arithmetic shift right the destination by the number of bits indicated by source.
	SAR,
	/// Logical shift right the destination by the number of bits indicated by source.
	SHR,

	/// Clear carry flag; no source or destination provided.
	CLC,
	/// Clear direction flag; no source or destination provided.
	CLD,
	/// Clear interrupt flag; no source or destination provided.
	CLI,
	/// Set carry flag.
	STC,
	/// Set decimal flag.
	STD,
	/// Set interrupt flag.
	STI,
	/// Complement carry flag; no source or destination provided.
	CMC,

	/// Compare; source, destination, operand and displacement will be populated appropriately.
	CMP,
	/// Sets flags based on the result of a logical AND of source and destination.
	TEST,

	/// Exchanges the contents of the source and destination.
	XCHG,

	/// Load AL with DS:[AL+BX].
	XLAT,

	// TODO: expand detail on all operations below.

	//
	// 80186 additions.
	//

	/// Checks an array index against bounds.
	BOUND,

	/// Create stack frame.
	ENTER,
	/// Procedure exit; copies BP to SP, then pops a new BP from the stack.
	LEAVE,

	/// Inputs from a port, incrementing or decrementing the destination.
	INS,
	/// Outputs to a port, incrementing or decrementing the destination.
	OUTS,

	/// Pushes all general purpose registers to the stack, in the order:
	/// AX, CX, DX, BX, [original] SP, BP, SI, DI.
	PUSHA,
	/// Pops all general purpose registers from the stack, in the reverse of
	/// the PUSHA order, i.e. DI, SI, BP, [final] SP, BX, DX, CX, AX.
	POPA,

	//
	// 80286 additions.
	//

	/// Adjusts requested privilege level.
	ARPL,
	/// Clears the task-switched flag.
	CLTS,
	/// Loads access rights.
	LAR,

	/// Loads the global descriptor table.
	LGDT,
	/// Loads the interrupt descriptor table.
	LIDT,
	/// Loads the local descriptor table.
	LLDT,
	/// Stores the global descriptor table.
	SGDT,
	/// Stores the interrupt descriptor table.
	SIDT,
	/// Stores the local descriptor table.
	SLDT,

	/// Verifies a segment for reading.
	VERR,
	/// Verifies a segment for writing.
	VERW,

	/// Loads the machine status word.
	LMSW,
	/// Stores the machine status word.
	SMSW,
	/// Loads a segment limit
	LSL,
	/// Loads the task register.
	LTR,
	/// Stores the task register.
	STR,

	/// Undocumented (but used); loads all registers, including internal ones.
	LOADALL,

	//
	// 80386 additions.
	//

	/// Loads a pointer to FS.
	LFS,
	/// Loads a pointer to GS.
	LGS,
	/// Loads a pointer to SS.
	LSS,

	/// Shift left double.
	SHLD,
	/// Shift right double.
	SHRD,

	/// Bit scan forwards.
	BSF,
	/// Bit scan reverse.
	BSR,
	/// Bit test.
	BT,
	/// Bit test and complement.
	BTC,
	/// Bit test and reset.
	BTR,
	/// Bit test and set.
	BTS,

	/// Compare string double word.
	CMPSD,
	/// [Early 80386s only] Insert bit string.
	IBITS,

	/// Inputs a double word from a port, incrementing or decrementing the destination.
	INSD,

	/// Convert dword to qword.
	CDQ,
	/// Convert word to dword; AX will be expanded to fill EAX.
	/// Compare and contrast to CWD which would expand AX to DX:AX.
	CWDE,
};

enum class Size: uint8_t {
	Implied = 0,
	Byte = 1,
	Word = 2,
	DWord = 4,
};

enum class Source: uint8_t {
	// These are in SIB order; this matters for packing later on.
	// Whether each refers to e.g. EAX, AX or AL depends on the
	// instruction's data size.
	eAX, eCX, eDX, eBX, eSP, eBP, eSI, eDI,

	// Selectors are provided as a group.
	CS, DS, ES, SS, FS, GS,

	DirectAddress,
	Immediate,
	Indirect,

	// Legacy 8-bit registers that can't be described as e.g. 8-bit eAX,
	// or where the source is 8-bit but the destination is 16-bit.
	CL,
	AH, BH, CH, DH,

	// TODO: can these all be eliminated in favour of eAX,2, etc?
	AX,
	DX,

	// TODO: compact and replace with a reference to a SIB.
	IndBXPlusSI,
	IndBXPlusDI,
	IndBPPlusSI,
	IndBPPlusDI,
	IndSI,
	IndDI,
	IndBP,
	IndBX,

	/// @c None can be treated as a source that produces 0 when encountered;
	/// it is semantically valid to receive it with that meaning in some contexts —
	/// e.g. to indicate no index in indirect addressing.
	None,
};

enum class Repetition: uint8_t {
	None, RepE, RepNE
};

/// Provides a 32-bit-style scale, index and base; to produce the address this represents,
/// calcluate base() + (index() << scale()).
///
/// This form of indirect addressing is used to describe both 16- and 32-bit indirect addresses,
/// even though it is a superset of that supported prior to the 80386.
class ScaleIndexBase {
	public:
		ScaleIndexBase(uint8_t sib) : sib_(sib) {}
		ScaleIndexBase(int scale, Source index, Source base) : sib_(uint8_t(scale << 6 | (int(index != Source::None ? index : Source::eSI) << 3) | int(base))) {}

		/// @returns the power of two by which to multiply @c index() before adding it to @c base().
		int scale() const {
			return sib_ >> 6;
		}

		/// @returns the @c index for this address; this is guaranteed to be one of eAX, eBX, eCX, eDX, None, eBP, eSI or eDI.
		Source index() const {
			constexpr Source sources[] = {
				Source::eAX, Source::eCX, Source::eDX, Source::eBX, Source::None, Source::eBP, Source::eSI, Source::eDI,
			};
			static_assert(sizeof(sources) == 8);
			return sources[(sib_ >> 3) & 0x7];
		}

		/// @returns the @c base for this address; this is guaranteed to be one of eAX, eBX, eCX, eDX, eSP, eBP, eSI or eDI.
		Source base() const {
			return Source(sib_ & 0x7);
		}

	private:
		// Data is stored directly as an 80386 SIB byte.
		const uint8_t sib_ = 0;
};
static_assert(sizeof(ScaleIndexBase) == 1);
static_assert(alignof(ScaleIndexBase) == 1);

class Instruction {
	public:
		Operation operation = Operation::Invalid;

		bool operator ==(const Instruction &rhs) const {
			return
				repetition_size_ == rhs.repetition_size_ &&
				sources_ == rhs.sources_ &&
				displacement_ == rhs.displacement_ &&
				operand_ == rhs.operand_;
		}

	private:
		// b0, b1: a Repetition;
		// b2+: operation size.
		uint8_t repetition_size_ = 0;

		// b0–b5: source;
		// b6–b11: destination;
		// b12–b14: segment override;
		// b15: lock.
		uint16_t sources_ = 0;

		// Unpackable fields.
		int16_t displacement_ = 0;
		uint16_t operand_ = 0;		// ... or used to store a segment for far operations.

	public:
		Source source() const			{	return Source(sources_ & 0x3f);				}
		Source destination() const		{	return Source((sources_ >> 6) & 0x3f);		}
		bool lock() const				{	return sources_ & 0x8000;					}
		Source segment_override() const	{	return Source((sources_ >> 12) & 7);		}

		Repetition repetition() const	{	return Repetition(repetition_size_ & 3);	}
		Size operation_size() const 	{	return Size(repetition_size_ >> 2);			}

		uint16_t segment() const		{	return uint16_t(operand_);					}
		uint16_t offset() const			{	return uint16_t(displacement_);				}

		int16_t displacement() const	{	return displacement_;						}
		uint16_t operand() const		{	return operand_;							}

		Instruction() noexcept {}
		Instruction(
			Operation operation,
			Source source,
			Source destination,
			bool lock,
			Source segment_override,
			Repetition repetition,
			Size operation_size,
			int16_t displacement,
			uint16_t operand) noexcept :
				operation(operation),
				repetition_size_(uint8_t((int(operation_size) << 2) | int(repetition))),
				sources_(uint16_t(
					int(source) |
					(int(destination) << 6) |
					(int(segment_override) << 12) |
					(int(lock) << 15)
				)),
				displacement_(displacement),
				operand_(operand) {}
};

static_assert(sizeof(Instruction) <= 8);

}
}

#endif /* InstructionSets_x86_Instruction_h */
