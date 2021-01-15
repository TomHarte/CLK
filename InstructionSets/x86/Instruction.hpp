//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/15/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_x86_Instruction_h
#define InstructionSets_x86_Instruction_h

#include <cstdint>

namespace CPU {
namespace Decoder {
namespace x86 {

/*
	Operations are documented below to establish expectations as to which
	instruction fields will be meaningful for each; this is a work-in-progress
	and may currently contain errors in the opcode descriptions — especially
	where implicit register dependencies are afoot.
*/
enum class Operation: uint8_t {
	Invalid,

	/// ASCII adjust after addition; source will be AL and destination will be AX.
	AAA,
	/// ASCII adjust before division; destination will be AX and source will be a multiplier.
	AAD,
	/// ASCII adjust after multiplication; destination will be AX and source will be a divider.
	AAM,
	/// ASCII adjust after subtraction; source will be AL and destination will be AX.
	AAS,
	/// Add with carry; source, destination, operand and displacement will be populated appropriately.
	ADC,
	/// Add; source, destination, operand and displacement will be populated appropriately.
	ADD,
	/// And; source, destination, operand and displacement will be populated appropriately.
	AND,
	/// Far call; see the segment() and offset() fields.
	CALLF,
	/// Displacement call; followed by a 16-bit operand providing a call offset.
	CALLD,
	/// Near call.
	CALLN,
	/// Convert byte into word; source will be AL, destination will be AH.
	CBW,
	/// Clear carry flag; no source or destination provided.
	CLC,
	/// Clear direction flag; no source or destination provided.
	CLD,
	/// Clear interrupt flag; no source or destination provided.
	CLI,
	/// Complement carry flag; no source or destination provided.
	CMC,
	/// Compare; source, destination, operand and displacement will be populated appropriately.
	CMP,
	/// Compare [bytes or words, per operation size]; source and destination implied to be DS:[SI] and ES:[DI].
	CMPS,
	/// Convert word to double word; source will be AX and destination will be DX.
	CWD,
	/// Decimal adjust after addition; source and destination will be AL.
	DAA,
	/// Decimal adjust after subtraction; source and destination will be AL.
	DAS,
	/// Decrement; source, destination, operand and displacement will be populated appropriately.
	DEC,
	/// Unsigned divide; divide the source value by AX or AL, storing the quotient in AL and the remainder in AH.
	DIV,
	/// Signed divide; divide the source value by AX or AL, storing the quotient in AL and the remainder in AH.
	IDIV,
	/// Escape, for a coprocessor; perform the bus cycles necessary to read the source and destination and perform a NOP.
	ESC,
	HLT,
	IMUL,
	IN,
	INC,
	INT,
	INT3,
	INTO,
	IRET,

	// Various jumps; see the displacement to calculate targets.
	JO,	JNO,	JB, JNB,	JE, JNE,	JBE, JNBE,
	JS, JNS,	JP, JNP,	JL, JNL,	JLE, JNLE,

	JMPN,
	JMPF,
	JCXZ,
	LAHF, LDS, LEA,
	LODS, LOOPE, LOOPNE, MOV, MOVS, MUL, NEG, NOP, NOT, OR, OUT,
	POP, POPF, PUSH, PUSHF, RCL, RCR, REP, ROL, ROR, SAHF,
	SAR, SBB, SCAS, SAL, SHR, STC, STD, STI, STOS, SUB, TEST,
	WAIT, XCHG, XLAT, XOR,
	LES, LOOP, JPCX,

	RETF,
	RETN,
};

enum class Size: uint8_t {
	Implied = 0,
	Byte = 1,
	Word = 2,
	DWord = 4,
};

enum class Source: uint8_t {
	None,
	CS, DS, ES, SS,

	AL, AH, AX,
	BL, BH, BX,
	CL, CH, CX,
	DL, DH, DX,

	SI, DI,
	BP, SP,

	IndBXPlusSI,
	IndBXPlusDI,
	IndBPPlusSI,
	IndBPPlusDI,
	IndSI,
	IndDI,
	DirectAddress,
	IndBP,
	IndBX,

	Immediate
};

enum class Repetition: uint8_t {
	None, RepE, RepNE
};

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
}

#endif /* InstructionSets_x86_Instruction_h */
