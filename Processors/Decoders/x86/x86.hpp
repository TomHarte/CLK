//
//  x86.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef x86_hpp
#define x86_hpp

#include <cstddef>
#include <cstdint>

namespace CPU {
namespace Decoder {
namespace x86 {

enum class Model {
	i8086,
};

enum class Operation: uint8_t {
	Invalid,

	AAA, AAD, AAM, AAS, ADC, ADD, AND, CALL, CBW, CLC, CLD, CLI, CMC,
	CMP, CMPS, CWD, DAA, DAS, DEC, DIV, ESC, HLT, IDIV, IMUL, IN,
	INC, INT, INTO, IRET,
	JO, JNO,
	JB, JNB,
	JE, JNE,
	JBE, JNBE,
	JS, JNS,
	JP, JNP,
	JL, JNL,
	JLE, JNLE,
	JMP, JCXZ,
	LAHF, LDS, LEA,
	LODS, LOOPE, LOOPNE, MOV, MOVS, MUL, NEG, NOP, NOT, OR, OUT,
	POP, POPF, PUSH, PUSHF, RCL, RCR, REP, ROL, ROR, SAHF,
	SAR, SBB, SCAS, SHL, SHR, STC, STD, STI, STOS, SUB, TEST,
	WAIT, XCHG, XLAT, XOR,

	RETInter,
	RETIntra,
};

enum class Size: uint8_t {
	Implied = 0,
	Byte = 1,
	Word = 2,
	DWord = 4,
};

enum class Source: uint8_t {
	None,

	AL, AH, AX,
	BL, BH, BX,
	CL, CH, CX,
	DL, DH, DX,

	CS, DS, ES, SS,
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

class Instruction {
	public:
		Operation operation = Operation::Invalid;
		Size operand_size = Size::Byte;

		Source source = Source::AL;
		Source destination = Source::AL;

		int size() const {
			return size_;
		}

		Instruction() {}
		Instruction(int size) : size_(size) {}
		Instruction(Operation operation, Size operand_size, Source source, Source destination, int size) :
			operation(operation), operand_size(operand_size), source(source), destination(destination), size_(size) {}

	private:
		int size_ = -1;
};

/*!
	Implements Intel x86 instruction decoding.

	This is an experimental implementation; it has not yet undergone significant testing.
*/
struct Decoder {
	public:
		Decoder(Model model);

		/*!
			@returns an @c Instruction with a positive size to indicate successful decoding; a
				negative size specifies the [negatived] number of further bytes the caller should ideally
				collect before calling again. The caller is free to call with fewer, but may not get a decoded
				instruction in response, and the decoder may still not be able to complete decoding
				even if given that number of bytes.
		*/
		Instruction decode(const uint8_t *source, size_t length);

	private:
		enum class Phase {
			/// Captures all prefixes and continues until an instruction byte is encountered.
			Instruction,
			/// Receives a ModRM byte and either populates the source_ and dest_ fields appropriately
			/// or completes decoding of the instruction, as per the instruction format.
			ModRM,
			/// Waits for sufficiently many bytes to pass for all associated operands to be captured.
			AwaitingOperands,
			/// Forms and returns an Instruction, and resets parsing state.
			ReadyToPost
		} phase_ = Phase::Instruction;

		/// During the ModRM phase, format dictates interpretation of the ModRM byte.
		///
		/// During the ReadyToPost phase, format determines how transiently-recorded fields
		/// are packaged into an Instruction.
		enum class Format: uint8_t {
			Implied,

			// In both cases: pass the ModRM for mode, register and register/memory
			// flags and populate the source_ and destination_ fields appropriate.
			// During the ModRM phase they'll be populated as source_ = register,
			// destination_ = register/memory; the ReadyToPost phase should switch
			// those around as necessary.
			MemReg_Reg,
			Reg_MemReg,

			Reg_Data,

			//
			Reg_Addr,
			Addr_Reg,

			SegReg_MemReg,

			Immediate,
		} format_ = Format::MemReg_Reg;
		// TODO: figure out which Formats can be folded together,
		// and which are improperly elided.

		// Ephemeral decoding state.
		Operation operation_ = Operation::Invalid;
		bool large_operand_ = false;
		Source source_ = Source::None;
		Source destination_ = Source::None;
		uint8_t instr_ = 0x00;
		bool add_offset_ = false;
		bool large_offset_ = false;

		// Prefix capture fields.
		enum class Repetition: uint8_t {
			None, RepE, RepNE
		} repetition_ = Repetition::None;
		bool lock_ = false;
		Source segment_override_ = Source::None;

		// Size capture.
		int consumed_ = 0;
		int operand_bytes_ = 0;

		/// Resets size capture and all fields with default values.
		void reset_parsing() {
			consumed_ = operand_bytes_ = 0;
			lock_ = false;
			segment_override_ = Source::None;
			repetition_ = Repetition::None;
		}
};

}
}
}

#endif /* x86_hpp */
