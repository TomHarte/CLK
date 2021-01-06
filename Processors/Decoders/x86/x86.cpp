//
//  x86.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "x86.hpp"

#include <algorithm>
#include <cassert>

using namespace CPU::Decoder::x86;

// Only 8086 is suppoted for now.
Decoder::Decoder(Model) {}

Instruction Decoder::decode(const uint8_t *source, size_t length) {
	const uint8_t *const end = source + length;

	// MARK: - Prefixes (if present) and the opcode.

/// Covers anything which is complete as soon as the opcode is encountered.
#define MapComplete(value, op, src, dest, size)		\
	case value:										\
		operation_ = Operation::op;					\
		source_ = Source::src;						\
		destination_ = Source::dest;				\
		phase_ = Phase::ReadyToPost;				\
		operation_size_ = size;						\
	break

/// Handles instructions of the form rr, kk and rr, jjkk, i.e. a destination register plus an operand.
#define MapRegData(value, op, dest, size)					\
	case value:												\
		operation_ = Operation::op;							\
		source_ = Source::Immediate;						\
		destination_ = Source::dest;						\
		phase_ = Phase::AwaitingDisplacementOrOperand;		\
		operand_size_ = size;								\
	break

/// Handles instructions of the form Ax, jjkk where the latter is implicitly an address.
#define MapRegAddr(value, op, dest, op_size, addr_size)		\
	case value:												\
		operation_ = Operation::op;							\
		destination_ = Source::dest;						\
		source_ = Source::DirectAddress;					\
		phase_ = Phase::AwaitingDisplacementOrOperand;		\
		operand_size_ = addr_size;							\
		operation_size_ = op_size;							\
	break

/// Handles instructions of the form jjkk, Ax where the former is implicitly an address.
#define MapAddrReg(value, op, source, op_size, addr_size)	\
	case value:												\
		operation_ = Operation::op;							\
		source_ = Source::source;							\
		destination_ = Source::DirectAddress;				\
		phase_ = Phase::AwaitingDisplacementOrOperand;		\
		operand_size_ = addr_size;							\
		operation_size_ = op_size;							\
	break

/// Covers both `mem/reg, reg` and `reg, mem/reg`.
#define MapMemRegReg(value, op, format, size)		\
	case value:										\
		operation_ = Operation::op;					\
		phase_ = Phase::ModRegRM;					\
		modregrm_format_ = ModRegRMFormat::format;	\
		operand_size_ = 0;							\
		operation_size_ = size;						\
	break

/// Handles JO, JNO, JB, etc — jumps with a single byte displacement.
#define MapJump(value, op)								\
	case value:											\
		operation_ = Operation::op;						\
		phase_ = Phase::AwaitingDisplacementOrOperand;	\
		operand_size_ = 1;								\
	break

/// Handles far CALL and far JMP — fixed four byte operand operations.
#define MapFar(value, op)								\
	case value:											\
		operation_ = Operation::op;						\
		phase_ = Phase::AwaitingDisplacementOrOperand;	\
		operand_size_ = 4;								\
	break

	while(phase_ == Phase::Instruction && source != end) {
		// Retain the instruction byte, in case additional decoding is deferred
		// to the ModRegRM byte.
		instr_ = *source;
		++source;
		++consumed_;

		switch(instr_) {
			default: {
				const Instruction instruction(consumed_);
				reset_parsing();
				return instruction;
			}

#define PartialBlock(start, operation)						\
	MapMemRegReg(start + 0x00, operation, MemReg_Reg, 1);	\
	MapMemRegReg(start + 0x01, operation, MemReg_Reg, 2);	\
	MapMemRegReg(start + 0x02, operation, Reg_MemReg, 1);	\
	MapMemRegReg(start + 0x03, operation, Reg_MemReg, 2);	\
	MapRegData(start + 0x04, operation, AL, 1);				\
	MapRegData(start + 0x05, operation, AX, 2);

			PartialBlock(0x00, ADD);
			MapComplete(0x06, PUSH, ES, None, 2);
			MapComplete(0x07, POP, ES, None, 2);

			PartialBlock(0x08, OR);
			MapComplete(0x0e, PUSH, CS, None, 2);
			// 0x0f: not used.

			PartialBlock(0x10, ADC);
			MapComplete(0x16, PUSH, SS, None, 2);
			MapComplete(0x17, POP, SS, None, 2);

			PartialBlock(0x18, SBB);
			MapComplete(0x1e, PUSH, DS, None, 2);
			MapComplete(0x1f, POP, DS, None, 2);

			PartialBlock(0x20, AND);
			case 0x26:	segment_override_ = Source::ES;	break;
			MapComplete(0x27, DAA, None, None, 1);

			PartialBlock(0x28, SUB);
			case 0x2e:	segment_override_ = Source::CS;	break;
			MapComplete(0x2f, DAS, None, None, 1);

			PartialBlock(0x30, XOR);
			case 0x36:	segment_override_ = Source::SS;	break;
			MapComplete(0x37, AAA, None, None, 1);

			PartialBlock(0x38, CMP);
			case 0x3e:	segment_override_ = Source::DS;	break;
			MapComplete(0x3f, AAS, None, None, 1);

#undef PartialBlock

#define RegisterBlock(start, operation)	\
	MapComplete(start + 0x00, operation, AX, AX, 2);	\
	MapComplete(start + 0x01, operation, CX, CX, 2);	\
	MapComplete(start + 0x02, operation, DX, DX, 2);	\
	MapComplete(start + 0x03, operation, BX, BX, 2);	\
	MapComplete(start + 0x04, operation, SP, SP, 2);	\
	MapComplete(start + 0x05, operation, BP, BP, 2);	\
	MapComplete(start + 0x06, operation, SI, SI, 2);	\
	MapComplete(start + 0x07, operation, DI, DI, 2);	\

			RegisterBlock(0x40, INC);
			RegisterBlock(0x48, DEC);
			RegisterBlock(0x50, PUSH);
			RegisterBlock(0x58, POP);

#undef RegisterBlock

			// 0x60–0x6f: not used.

			MapJump(0x70, JO);
			MapJump(0x71, JNO);
			MapJump(0x72, JB);
			MapJump(0x73, JNB);
			MapJump(0x74, JE);
			MapJump(0x75, JNE);
			MapJump(0x76, JBE);
			MapJump(0x77, JNBE);
			MapJump(0x78, JS);
			MapJump(0x79, JNS);
			MapJump(0x7a, JP);
			MapJump(0x7b, JNP);
			MapJump(0x7c, JL);
			MapJump(0x7d, JNL);
			MapJump(0x7e, JLE);
			MapJump(0x7f, JNLE);

			// TODO:
			//
			//	0x80, 0x81, 0x82, 0x83, which all require more
			//	input, from the ModRegRM byte.

			MapMemRegReg(0x84, TEST, MemReg_Reg, 1);
			MapMemRegReg(0x85, TEST, MemReg_Reg, 2);
			MapMemRegReg(0x86, XCHG, Reg_MemReg, 1);
			MapMemRegReg(0x87, XCHG, Reg_MemReg, 2);
			MapMemRegReg(0x88, MOV, MemReg_Reg, 1);
			MapMemRegReg(0x89, MOV, MemReg_Reg, 2);
			MapMemRegReg(0x8a, MOV, Reg_MemReg, 1);
			MapMemRegReg(0x8b, MOV, Reg_MemReg, 2);
			// 0x8c: not used.
			MapMemRegReg(0x8d, LEA, Reg_MemReg, 2);
//			MapMemRegReg(0x8e, MOV, SegReg_MemReg, 1);	// TODO: SegReg_MemReg

			// TODO: 0x8f, which requires further selection from the ModRegRM byte.

			MapComplete(0x90, NOP, None, None, 0);	// Or XCHG AX, AX?
			MapComplete(0x91, XCHG, AX, CX, 2);
			MapComplete(0x92, XCHG, AX, DX, 2);
			MapComplete(0x93, XCHG, AX, BX, 2);
			MapComplete(0x94, XCHG, AX, SP, 2);
			MapComplete(0x95, XCHG, AX, BP, 2);
			MapComplete(0x96, XCHG, AX, SI, 2);
			MapComplete(0x97, XCHG, AX, DI, 2);

			MapComplete(0x98, CBW, None, None, 1);
			MapComplete(0x99, CWD, None, None, 2);
			MapFar(0x9a, CALL);
			MapComplete(0x9b, WAIT, None, None, 0);
			MapComplete(0x9c, PUSHF, None, None, 2);
			MapComplete(0x9d, POPF, None, None, 2);
			MapComplete(0x9e, SAHF, None, None, 1);
			MapComplete(0x9f, LAHF, None, None, 1);

			MapRegAddr(0xa0, MOV, AL, 1, 1);	MapRegAddr(0xa1, MOV, AX, 2, 2);
			MapAddrReg(0xa2, MOV, AL, 1, 1);	MapAddrReg(0xa3, MOV, AX, 2, 2);

			MapComplete(0xa4, MOVS, None, None, 1);
			MapComplete(0xa5, MOVS, None, None, 2);
			MapComplete(0xa6, CMPS, None, None, 1);
			MapComplete(0xa7, CMPS, None, None, 2);

			MapComplete(0xaa, STOS, None, None, 1);
			MapComplete(0xab, STOS, None, None, 2);
			MapComplete(0xac, LODS, None, None, 1);
			MapComplete(0xad, LODS, None, None, 2);
			MapComplete(0xae, SCAS, None, None, 1);
			MapComplete(0xaf, SCAS, None, None, 2);

			MapRegData(0xb0, MOV, AL, 1);	MapRegData(0xb1, MOV, CL, 1);
			MapRegData(0xb2, MOV, DL, 1);	MapRegData(0xb3, MOV, BL, 1);
			MapRegData(0xb4, MOV, AH, 1);	MapRegData(0xb5, MOV, CH, 1);
			MapRegData(0xb6, MOV, DH, 1);	MapRegData(0xb7, MOV, BH, 1);
			MapRegData(0xb8, MOV, AX, 2);	MapRegData(0xb9, MOV, CX, 2);
			MapRegData(0xba, MOV, DX, 2);	MapRegData(0xbb, MOV, BX, 2);
			MapRegData(0xbc, MOV, SP, 2);	MapRegData(0xbd, MOV, BP, 2);
			MapRegData(0xbe, MOV, SI, 2);	MapRegData(0xbf, MOV, DI, 2);

			MapRegData(0xc2, RETIntra, None, 2);
			MapComplete(0xc3, RETIntra, None, None, 2);

			MapMemRegReg(0xc4, LES, Reg_MemReg, 4);
			MapMemRegReg(0xc5, LDS, Reg_MemReg, 4);

			MapRegData(0xca, RETInter, None, 2);
			MapComplete(0xcb, RETInter, None, None, 4);

			MapComplete(0xcf, IRET, None, None, 0);

			MapRegData(0xd4, AAM, None, 1);
			MapRegData(0xd5, AAD, None, 1);

			MapMemRegReg(0xd8, ESC, MemReg_Reg, 0);
			MapMemRegReg(0xd9, ESC, MemReg_Reg, 0);
			MapMemRegReg(0xda, ESC, MemReg_Reg, 0);
			MapMemRegReg(0xdb, ESC, MemReg_Reg, 0);
			MapMemRegReg(0xdc, ESC, MemReg_Reg, 0);
			MapMemRegReg(0xdd, ESC, MemReg_Reg, 0);
			MapMemRegReg(0xde, ESC, MemReg_Reg, 0);
			MapMemRegReg(0xdf, ESC, MemReg_Reg, 0);

			MapRegAddr(0xe4, IN, AL, 1, 1);		MapRegAddr(0xe5, IN, AX, 2, 1);
			MapAddrReg(0xe6, OUT, AL, 1, 1);	MapAddrReg(0xe7, OUT, AX, 2, 1);

			MapRegData(0xe8, CALL, None, 2);
			MapRegData(0xe9, JMP, None, 2);

			MapFar(0xea, JMP);

			MapComplete(0xec, IN, DX, AL, 1);	MapComplete(0xed, IN, DX, AX, 1);
			MapComplete(0xee, OUT, AL, DX, 1);	MapComplete(0xef, OUT, AX, DX, 1);

			MapMemRegReg(0xf6, Invalid, MemRegTEST_to_IDIV, 1);
			MapMemRegReg(0xf7, Invalid, MemRegTEST_to_IDIV, 2);

			MapComplete(0xf9, STC, None, None, 1);
			MapComplete(0xfd, STD, None, None, 1);

			// Other prefix bytes.
			case 0xf0:	lock_ = true;						break;
			case 0xf2:	repetition_ = Repetition::RepNE;	break;
			case 0xf3:	repetition_ = Repetition::RepE;		break;
		}
	}

#undef MapInstr

	// MARK: - ModRegRM byte, if any.

	if(phase_ == Phase::ModRegRM && source != end) {
		const uint8_t mod = *source >> 6;		// i.e. mode.
		const uint8_t reg = (*source >> 3) & 7;	// i.e. register.
		const uint8_t rm = *source & 7;			// i.e. register/memory.
		++source;
		++consumed_;

		Source memreg;
		constexpr Source reg_table[3][8] = {
			{},
			{
				Source::AL,	Source::CL,	Source::DL,	Source::BL,
				Source::AH,	Source::CH,	Source::DH,	Source::BH,
			}, {
				Source::AX,	Source::CX,	Source::DX,	Source::BX,
				Source::SP,	Source::BP,	Source::SI,	Source::DI,
			}
		};
		switch(mod) {
			case 0: {
				constexpr Source rm_table[8] = {
					Source::IndBXPlusSI,	Source::IndBXPlusDI,
					Source::IndBPPlusSI,	Source::IndBPPlusDI,
					Source::IndSI,			Source::IndDI,
					Source::DirectAddress,	Source::IndBX,
				};
				memreg = rm_table[rm];
			} break;

			default: {
				constexpr Source rm_table[8] = {
					Source::IndBXPlusSI,	Source::IndBXPlusDI,
					Source::IndBPPlusSI,	Source::IndBPPlusDI,
					Source::IndSI,			Source::IndDI,
					Source::IndBP,			Source::IndBX,
				};
				memreg = rm_table[rm];

				displacement_size_ = 1 + (mod == 2);
			} break;

			// Other operand is just a register.
			case 3:
				memreg = reg_table[operation_size_][rm];
			break;
		}

		switch(modregrm_format_) {
			case ModRegRMFormat::Reg_MemReg:
			case ModRegRMFormat::MemReg_Reg: {
				if(modregrm_format_ == ModRegRMFormat::Reg_MemReg) {
					source_ = memreg;
					destination_ = reg_table[operation_size_][reg];
				} else {
					source_ = reg_table[operation_size_][reg];
					destination_ = memreg;
				}
			} break;

			case ModRegRMFormat::MemRegTEST_to_IDIV:
				source_ = destination_ = memreg;

				switch(reg) {
					default:
						reset_parsing();
					return Instruction();

					case 0: 	operation_ = Operation::TEST;	break;
					case 2: 	operation_ = Operation::NOT;	break;
					case 3: 	operation_ = Operation::NEG;	break;
					case 4: 	operation_ = Operation::MUL;	break;
					case 5: 	operation_ = Operation::IMUL;	break;
					case 6: 	operation_ = Operation::DIV;	break;
					case 7: 	operation_ = Operation::IDIV;	break;
				}
			break;

			default: assert(false);
		}

		phase_ = Phase::AwaitingDisplacementOrOperand;
	}

	// MARK: - Displacement and operand.

	if(phase_ == Phase::AwaitingDisplacementOrOperand && source != end) {
		// TODO: calculate number of expected operands.
		const int required_bytes = displacement_size_ + operand_size_;

		const int outstanding_bytes = required_bytes - operand_bytes_;
		const int bytes_to_consume = std::min(int(end - source), outstanding_bytes);
		source += bytes_to_consume;
		consumed_ += bytes_to_consume;
		operand_bytes_ += bytes_to_consume;
		if(bytes_to_consume == outstanding_bytes) {
			phase_ = Phase::ReadyToPost;
		} else {
			// Provide a genuine measure of further bytes required.
			return Instruction(-(outstanding_bytes - bytes_to_consume));
		}
	}

	// MARK: - Check for completion.

	if(phase_ == Phase::ReadyToPost) {
		Instruction result(operation_, Size(operation_size_), source_, destination_, consumed_);
		reset_parsing();
		phase_ = Phase::Instruction;
		return result;
	}

	// i.e. not done yet.
	return Instruction();
}
