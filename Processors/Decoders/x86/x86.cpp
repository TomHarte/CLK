//
//  x86.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "x86.hpp"

#include <cassert>

using namespace CPU::Decoder::x86;

// Only 8086 is suppoted for now.
Decoder::Decoder(Model) {}

Instruction Decoder::decode(uint8_t *source, size_t length) {
	uint8_t *const limit = source + length;

#define MapPartial(value, op, lrg, fmt, phs)	\
	case value:									\
		operation_ = Operation::op;				\
		large_operand_ = lrg;					\
		format_ = Format::fmt;					\
		phase_ = Phase::phs;					\
	break

#define MapComplete(value, op, src, dest)		\
	case value:									\
		operation_ = Operation::op;				\
		source_ = Source::src;					\
		destination_ = Source::dest;			\
		phase_ = Phase::ReadyToPost;			\
	break

	while(phase_ == Phase::Instruction && source != limit) {
		// Retain the instruction byte, in case additional decoding is deferred
		// to the ModRM byte.
		instr_ = *source;
		switch(instr_) {
#define PartialBlock(start, operation)	\
	MapPartial(start + 0x00, operation, false, MemReg_Reg, ModRM);			\
	MapPartial(start + 0x01, operation, true, MemReg_Reg, ModRM);			\
	MapPartial(start + 0x02, operation, false, Reg_MemReg, ModRM);			\
	MapPartial(start + 0x03, operation, true, Reg_MemReg, ModRM);			\
	MapPartial(start + 0x04, operation, false, Ac_Data, AwaitingOperands);	\
	MapPartial(start + 0x05, operation, true, Ac_Data, AwaitingOperands);

			PartialBlock(0x00, ADD);
			MapComplete(0x06, PUSH, ES, None);
			MapComplete(0x07, POP, ES, None);

			PartialBlock(0x08, OR);
			MapComplete(0x0e, PUSH, CS, None);
			/* 0x0f: not used. */

			PartialBlock(0x10, ADC);
			MapComplete(0x16, PUSH, SS, None);
			MapComplete(0x17, POP, SS, None);

			PartialBlock(0x18, SBB);
			MapComplete(0x1e, PUSH, DS, None);
			MapComplete(0x1f, POP, DS, None);

			PartialBlock(0x20, AND);
			case 0x26:	segment_override_ = Source::ES;	break;
			MapComplete(0x27, DAA, None, None);

			PartialBlock(0x28, SUB);
			case 0x2e:	segment_override_ = Source::CS;	break;
			MapComplete(0x2f, DAS, None, None);

			PartialBlock(0x30, XOR);
			case 0x36:	segment_override_ = Source::SS;	break;
			MapComplete(0x37, AAA, None, None);

			PartialBlock(0x38, CMP);
			case 0x3e:	segment_override_ = Source::DS;	break;
			MapComplete(0x3f, AAS, None, None);

#undef PartialBlock

#define RegisterBlock(start, operation)	\
	MapComplete(start + 0x00, operation, AX, AX);	\
	MapComplete(start + 0x01, operation, CX, CX);	\
	MapComplete(start + 0x02, operation, DX, DX);	\
	MapComplete(start + 0x03, operation, BX, BX);	\
	MapComplete(start + 0x04, operation, SP, SP);	\
	MapComplete(start + 0x05, operation, BP, BP);	\
	MapComplete(start + 0x06, operation, SI, SI);	\
	MapComplete(start + 0x07, operation, DI, DI);	\

			RegisterBlock(0x40, INC);
			RegisterBlock(0x48, DEC);
			RegisterBlock(0x50, PUSH);
			RegisterBlock(0x58, POP);

#undef RegisterBlock

			/* 0x60–0x6f: not used. */

#define MapJump(value, operation)	MapPartial(value, operation, false, Disp, AwaitingOperands);
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
#undef MapJump

			// TODO:
			//
			//	0x80, 0x81, 0x82, 0x83, which all require more
			//	input, from the ModRM byte.

			MapPartial(0x84, TEST, false, MemReg_Reg, ModRM);
			MapPartial(0x85, TEST, true, MemReg_Reg, ModRM);
			MapPartial(0x86, XCHG, false, Reg_MemReg, ModRM);
			MapPartial(0x87, XCHG, true, Reg_MemReg, ModRM);
			MapPartial(0x88, MOV, false, MemReg_Reg, ModRM);
			MapPartial(0x89, MOV, true, MemReg_Reg, ModRM);
			MapPartial(0x8a, MOV, false, Reg_MemReg, ModRM);
			MapPartial(0x8b, MOV, true, Reg_MemReg, ModRM);
			/* 0x8c: not used. */
			MapPartial(0x8d, LEA, true, Reg_Addr, ModRM);
			MapPartial(0x8e, MOV, true, SegReg_MemReg, ModRM);

			// TODO: 0x8f, which requires further selection from the ModRM byte.

			MapComplete(0x90, NOP, None, None);	// Or XCHG AX, AX?
			MapComplete(0x91, XCHG, AX, CX);
			MapComplete(0x92, XCHG, AX, DX);
			MapComplete(0x93, XCHG, AX, BX);
			MapComplete(0x94, XCHG, AX, SP);
			MapComplete(0x95, XCHG, AX, BP);
			MapComplete(0x96, XCHG, AX, SI);
			MapComplete(0x97, XCHG, AX, DI);

			MapComplete(0x98, CBW, None, None);
			MapComplete(0x99, CWD, None, None);
			MapPartial(0x9a, CALL, true, Addr, AwaitingOperands);
			MapComplete(0x9b, WAIT, None, None);
			MapComplete(0x9c, PUSHF, None, None);
			MapComplete(0x9d, POPF, None, None);
			MapComplete(0x9e, SAHF, None, None);
			MapComplete(0x9f, LAHF, None, None);

			MapPartial(0xa0, MOV, false, Reg_Addr, AwaitingOperands);

			// Other prefix bytes.
			case 0xf0:	lock_ = true;						break;
			case 0xf2:	repetition_ = Repetition::RepNE;	break;
			case 0xf3:	repetition_ = Repetition::RepE;		break;
		}
		++source;
		++consumed_;
	}

#undef MapInstr

	if(phase_ == Phase::ModRM && source != limit) {
		const uint8_t mod = *source >> 6;		// i.e. mode.
		const uint8_t reg = (*source >> 3) & 7;	// i.e. register.
		const uint8_t rm = *source & 7;			// i.e. register/memory.

		switch(format_) {
			case Format::Reg_MemReg:
			case Format::MemReg_Reg: {
				Source memreg;

				constexpr Source reg_table[2][8] = {
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
						add_offset_ = false;
						constexpr Source rm_table[8] = {
							Source::IndBXPlusSI,	Source::IndBXPlusDI,
							Source::IndBPPlusSI,	Source::IndBPPlusDI,
							Source::IndSI,			Source::IndDI,
							Source::DirectAddress,	Source::IndBX,
						};
						memreg = rm_table[rm];
					} break;

					default: {
						add_offset_ = true;
						large_offset_ = (mod == 2);
						constexpr Source rm_table[8] = {
							Source::IndBXPlusSI,	Source::IndBXPlusDI,
							Source::IndBPPlusSI,	Source::IndBPPlusDI,
							Source::IndSI,			Source::IndDI,
							Source::IndBP,			Source::IndBX,
						};
						memreg = rm_table[rm];
					} break;

					// Other operand is just a register.
					case 3:	memreg = reg_table[large_operand_][rm];	break;
				}

				if(format_ == Format::Reg_MemReg) {
					destination_ = reg_table[large_operand_][reg];
					source_ = memreg;
				} else {
					source_ = reg_table[large_operand_][reg];
					destination_ = memreg;
				}
				phase_ = (add_offset_ || memreg == Source::DirectAddress) ? Phase::AwaitingOperands : Phase::ReadyToPost;
			} break;

			default: assert(false);
		}

		++source;
		++consumed_;
	}

	if(phase_ == Phase::AwaitingOperands && source != limit) {
		// TODO: calculate number of expected operands.
	}

	if(phase_ == Phase::ReadyToPost) {
		// TODO: construct actual Instruction.
	}

	return Instruction();
}
