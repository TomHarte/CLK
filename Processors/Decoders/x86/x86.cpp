//
//  x86.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 1/1/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "x86.hpp"

using namespace CPU::Decoder::x86;

Instruction Decoder::decode(uint8_t *source, size_t length) {
	uint8_t *const limit = source + length;

#define MapPartial(value, op, sz, fmt, phs)	\
	case value:								\
		operation_ = Operation::op;			\
		operand_size_ = Size::sz;			\
		format_ = Format::fmt;				\
		phase_ = Phase::phs;				\
	break;

#define MapComplete(value, op, sz, src, dest)		\
	case value:										\
		operation_ = Operation::op;					\
		operand_size_ = Size::sz;					\
		source_ = Source::src;						\
		destination_ = Source::dest;				\
		phase_ = Phase::ReadyToPost;				\
	break;

	while(phase_ == Phase::Instruction && source != limit) {
		switch(*source) {
#define PartialBlock(start, operation)	\
	MapPartial(start + 0x00, operation, Byte, MemReg_Reg, ModRM);			\
	MapPartial(start + 0x01, operation, Word, MemReg_Reg, ModRM);			\
	MapPartial(start + 0x02, operation, Byte, Reg_MemReg, ModRM);			\
	MapPartial(start + 0x03, operation, Word, Reg_MemReg, ModRM);			\
	MapPartial(start + 0x04, operation, Byte, Ac_Data, AwaitingOperands);	\
	MapPartial(start + 0x05, operation, Word, Ac_Data, AwaitingOperands);

			PartialBlock(0x00, ADD);
			MapComplete(0x06, PUSH, Word, ES, None);
			MapComplete(0x07, POP, Word, ES, None);

			PartialBlock(0x08, OR);
			MapComplete(0x0e, PUSH, Word, CS, None);
			/* 0x0f: not used. */

			PartialBlock(0x10, ADC);
			MapComplete(0x16, PUSH, Word, SS, None);
			MapComplete(0x17, POP, Word, SS, None);

			PartialBlock(0x18, SBB);
			MapComplete(0x1e, PUSH, Word, DS, None);
			MapComplete(0x1f, POP, Word, DS, None);

			PartialBlock(0x20, AND);
			case 0x26:	segment_override_ = Source::ES;	break;
			MapComplete(0x27, DAA, Implied, None, None);

			PartialBlock(0x28, SUB);
			case 0x2e:	segment_override_ = Source::CS;	break;
			MapComplete(0x2f, DAS, Implied, None, None);

			PartialBlock(0x30, XOR);
			case 0x36:	segment_override_ = Source::SS;	break;
			MapComplete(0x37, AAA, Implied, None, None);

			PartialBlock(0x38, CMP);
			case 0x3e:	segment_override_ = Source::DS;	break;
			MapComplete(0x3f, AAS, Implied, None, None);

#undef PartialBlock

#define RegisterBlock(start, operation)	\
	MapComplete(start + 0x00, operation, Word, AX, AX);	\
	MapComplete(start + 0x01, operation, Word, CX, CX);	\
	MapComplete(start + 0x02, operation, Word, DX, DX);	\
	MapComplete(start + 0x03, operation, Word, BX, BX);	\
	MapComplete(start + 0x04, operation, Word, SP, SP);	\
	MapComplete(start + 0x05, operation, Word, BP, BP);	\
	MapComplete(start + 0x06, operation, Word, SI, SI);	\
	MapComplete(start + 0x07, operation, Word, DI, DI);	\

			RegisterBlock(0x40, INC);
			RegisterBlock(0x48, DEC);
			RegisterBlock(0x50, PUSH);
			RegisterBlock(0x58, POP);

#undef RegisterBlock

			/* 0x60–0x6f: not used. */

			MapPartial(0x70, JO, Byte, Disp, AwaitingOperands);
			MapPartial(0x71, JNO, Byte, Disp, AwaitingOperands);
			MapPartial(0x72, JB, Byte, Disp, AwaitingOperands);
			MapPartial(0x73, JNB, Byte, Disp, AwaitingOperands);
			MapPartial(0x74, JE, Byte, Disp, AwaitingOperands);
			MapPartial(0x75, JNE, Byte, Disp, AwaitingOperands);
			MapPartial(0x76, JBE, Byte, Disp, AwaitingOperands);
			MapPartial(0x77, JNBE, Byte, Disp, AwaitingOperands);
			MapPartial(0x78, JS, Byte, Disp, AwaitingOperands);
			MapPartial(0x79, JNS, Byte, Disp, AwaitingOperands);
			MapPartial(0x7a, JP, Byte, Disp, AwaitingOperands);
			MapPartial(0x7b, JNP, Byte, Disp, AwaitingOperands);
			MapPartial(0x7c, JL, Byte, Disp, AwaitingOperands);
			MapPartial(0x7d, JNL, Byte, Disp, AwaitingOperands);
			MapPartial(0x7e, JLE, Byte, Disp, AwaitingOperands);
			MapPartial(0x7f, JNLE, Byte, Disp, AwaitingOperands);

		}
		++source;
		++consumed_;
	}

#undef MapInstr


	if(phase_ == Phase::ReadyToPost) {
		// TODO: construct actual Instruction.
	}

	return Instruction();
}
