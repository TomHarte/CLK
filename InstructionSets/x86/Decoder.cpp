//
//  x86.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"

#include <algorithm>
#include <cassert>
#include <utility>

using namespace InstructionSet::x86;

template <Model model>
std::pair<int, typename Decoder<model>::InstructionT> Decoder<model>::decode(const uint8_t *source, size_t length) {
	const uint8_t *const end = source + length;

	// MARK: - Prefixes (if present) and the opcode.

/// Helper macro for those that follow.
#define SetOpSrcDestSize(op, src, dest, size)	\
	operation_ = Operation::op;			\
	source_ = Source::src;				\
	destination_ = Source::dest;		\
	operation_size_ = size

/// Covers anything which is complete as soon as the opcode is encountered.
#define Complete(op, src, dest, size)		\
	SetOpSrcDestSize(op, src, dest, size);	\
	phase_ = Phase::ReadyToPost

/// Handles instructions of the form rr, kk and rr, jjkk, i.e. a destination register plus an operand.
#define RegData(op, dest, size)							\
	SetOpSrcDestSize(op, DirectAddress, dest, size);	\
	source_ = Source::Immediate;						\
	operand_size_ = size;								\
	phase_ = Phase::DisplacementOrOperand

/// Handles instructions of the form Ax, jjkk where the latter is implicitly an address.
#define RegAddr(op, dest, op_size, addr_size)			\
	SetOpSrcDestSize(op, DirectAddress, dest, op_size);	\
	operand_size_ = addr_size;							\
	phase_ = Phase::DisplacementOrOperand

/// Handles instructions of the form jjkk, Ax where the former is implicitly an address.
#define AddrReg(op, source, op_size, addr_size)				\
	SetOpSrcDestSize(op, source, DirectAddress, op_size);	\
	operand_size_ = addr_size;								\
	destination_ = Source::DirectAddress;					\
	phase_ = Phase::DisplacementOrOperand

/// Covers both `mem/reg, reg` and `reg, mem/reg`.
#define MemRegReg(op, format, size)				\
	operation_ = Operation::op;					\
	phase_ = Phase::ModRegRM;					\
	modregrm_format_ = ModRegRMFormat::format;	\
	operand_size_ = DataSize::None;				\
	operation_size_ = size

/// Handles JO, JNO, JB, etc — jumps with a single byte displacement.
#define Jump(op, size)								\
	operation_ = Operation::op;						\
	phase_ = Phase::DisplacementOrOperand;			\
	displacement_size_ = size

/// Handles far CALL and far JMP — fixed four byte operand operations.
#define Far(op)										\
	operation_ = Operation::op;						\
	phase_ = Phase::DisplacementOrOperand;			\
	operand_size_ = data_size_;						\
	displacement_size_ = DataSize::Word

/// Handles ENTER — a fixed three-byte operation.
#define Displacement16Operand8(op)					\
	operation_ = Operation::op;						\
	phase_ = Phase::DisplacementOrOperand;			\
	displacement_size_ = DataSize::Word;			\
	operand_size_ = DataSize::Byte

#define undefined()	{												\
	const auto result = std::make_pair(consumed_, InstructionT());	\
	reset_parsing();												\
	return result;													\
}

#define Requires(x)		if constexpr (model != Model::x) undefined();
#define RequiresMin(x)	if constexpr (model < Model::x) undefined();

	while(phase_ == Phase::Instruction && source != end) {
		const uint8_t instr = *source;
		++source;
		++consumed_;

		switch(instr) {
			default: undefined();

#define PartialBlock(start, operation)												\
	case start + 0x00: MemRegReg(operation, MemReg_Reg, DataSize::Byte);	break;	\
	case start + 0x01: MemRegReg(operation, MemReg_Reg, data_size_);		break;	\
	case start + 0x02: MemRegReg(operation, Reg_MemReg, DataSize::Byte);	break;	\
	case start + 0x03: MemRegReg(operation, Reg_MemReg, data_size_);		break;	\
	case start + 0x04: RegData(operation, eAX, DataSize::Byte);				break;	\
	case start + 0x05: RegData(operation, eAX, data_size_)

			PartialBlock(0x00, ADD);							break;
			case 0x06: Complete(PUSH, ES, None, data_size_);	break;
			case 0x07: Complete(POP, None, ES, data_size_);		break;

			PartialBlock(0x08, OR);								break;
			case 0x0e: Complete(PUSH, CS, None, data_size_);	break;

			// The 286 onwards have a further set of instructions
			// prefixed with $0f.
			case 0x0f:
				RequiresMin(i80286);
				phase_ = Phase::InstructionPageF;
			break;

			PartialBlock(0x10, ADC);								break;
			case 0x16: Complete(PUSH, SS, None, DataSize::Word);	break;
			case 0x17: Complete(POP, None, SS, DataSize::Word);		break;

			PartialBlock(0x18, SBB);								break;
			case 0x1e: Complete(PUSH, DS, None, DataSize::Word);	break;
			case 0x1f: Complete(POP, None, DS, DataSize::Word);		break;

			PartialBlock(0x20, AND);								break;
			case 0x26: segment_override_ = Source::ES;				break;
			case 0x27: Complete(DAA, eAX, eAX, DataSize::Byte);		break;

			PartialBlock(0x28, SUB);								break;
			case 0x2e: segment_override_ = Source::CS;				break;
			case 0x2f: Complete(DAS, eAX, eAX, DataSize::Byte);		break;

			PartialBlock(0x30, XOR);								break;
			case 0x36: segment_override_ = Source::SS;				break;
			case 0x37: Complete(AAA, eAX, eAX, DataSize::Word);		break;

			PartialBlock(0x38, CMP);								break;
			case 0x3e: segment_override_ = Source::DS;				break;
			case 0x3f: Complete(AAS, eAX, eAX, DataSize::Word);		break;

#undef PartialBlock

#define RegisterBlock(start, operation)										\
	case start + 0x00: Complete(operation, eAX, eAX, data_size_);	break;	\
	case start + 0x01: Complete(operation, eCX, eCX, data_size_);	break;	\
	case start + 0x02: Complete(operation, eDX, eDX, data_size_);	break;	\
	case start + 0x03: Complete(operation, eBX, eBX, data_size_);	break;	\
	case start + 0x04: Complete(operation, eSP, eSP, data_size_);	break;	\
	case start + 0x05: Complete(operation, eBP, eBP, data_size_);	break;	\
	case start + 0x06: Complete(operation, eSI, eSI, data_size_);	break;	\
	case start + 0x07: Complete(operation, eDI, eDI, data_size_)

			RegisterBlock(0x40, INC);	break;
			RegisterBlock(0x48, DEC);	break;
			RegisterBlock(0x50, PUSH);	break;
			RegisterBlock(0x58, POP);	break;

#undef RegisterBlock

			case 0x60:
				RequiresMin(i80186);
				Complete(PUSHA, None, None, data_size_);
			break;
			case 0x61:
				RequiresMin(i80186);
				Complete(POPA, None, None, data_size_);
			break;
			case 0x62:
				RequiresMin(i80186);
				MemRegReg(BOUND, Reg_MemReg, data_size_);
			break;
			case 0x63:
				RequiresMin(i80286);
				MemRegReg(ARPL, MemReg_Reg, DataSize::Word);
			break;
			case 0x64:
				RequiresMin(i80386);
				segment_override_ = Source::FS;
			break;
			case 0x65:
				RequiresMin(i80386);
				segment_override_ = Source::GS;
			break;
			case 0x66:
				RequiresMin(i80386);
				data_size_ = DataSize(int(default_data_size_) ^ int(DataSize::Word) ^ int(DataSize::DWord));
			break;
			case 0x67:
				RequiresMin(i80386);
				address_size_ = AddressSize(int(default_address_size_) ^ int(AddressSize::b16) ^ int(AddressSize::b32));
			break;
			// TODO: 0x68: PUSH Iv
			// TODO: 0x69: PUSH GvEvIv
			// TODO: 0x6a: PUSH Ib
			// TODO: 0x6b: IMUL GvEvIv
			case 0x6c:	// INSB
				RequiresMin(i80186);
				Complete(INS, None, None, DataSize::Byte);
			break;
			case 0x6d:	// INSW
				RequiresMin(i80186);
				Complete(INS, None, None, data_size_);
			break;
			case 0x6e:	// OUTSB
				RequiresMin(i80186);
				Complete(OUTS, None, None, DataSize::Byte);
			break;
			case 0x6f:	// OUTSW
				RequiresMin(i80186);
				Complete(OUTS, None, None, data_size_);
			break;

			case 0x70: Jump(JO, DataSize::Byte);	break;
			case 0x71: Jump(JNO, DataSize::Byte);	break;
			case 0x72: Jump(JB, DataSize::Byte);	break;
			case 0x73: Jump(JNB, DataSize::Byte);	break;
			case 0x74: Jump(JE, DataSize::Byte);	break;
			case 0x75: Jump(JNE, DataSize::Byte);	break;
			case 0x76: Jump(JBE, DataSize::Byte);	break;
			case 0x77: Jump(JNBE, DataSize::Byte);	break;
			case 0x78: Jump(JS, DataSize::Byte);	break;
			case 0x79: Jump(JNS, DataSize::Byte);	break;
			case 0x7a: Jump(JP, DataSize::Byte);	break;
			case 0x7b: Jump(JNP, DataSize::Byte);	break;
			case 0x7c: Jump(JL, DataSize::Byte);	break;
			case 0x7d: Jump(JNL, DataSize::Byte);	break;
			case 0x7e: Jump(JLE, DataSize::Byte);	break;
			case 0x7f: Jump(JNLE, DataSize::Byte);	break;

			case 0x80: MemRegReg(Invalid, MemRegADD_to_CMP, DataSize::Byte);			break;
			case 0x81: MemRegReg(Invalid, MemRegADD_to_CMP, data_size_);				break;
			case 0x82: MemRegReg(Invalid, MemRegADD_to_CMP_SignExtend, DataSize::Byte);	break;
			case 0x83: MemRegReg(Invalid, MemRegADD_to_CMP_SignExtend, data_size_);		break;

			case 0x84: MemRegReg(TEST, MemReg_Reg, DataSize::Byte);	break;
			case 0x85: MemRegReg(TEST, MemReg_Reg, data_size_);		break;
			case 0x86: MemRegReg(XCHG, Reg_MemReg, DataSize::Byte);	break;
			case 0x87: MemRegReg(XCHG, Reg_MemReg, data_size_);		break;
			case 0x88: MemRegReg(MOV, MemReg_Reg, DataSize::Byte);	break;
			case 0x89: MemRegReg(MOV, MemReg_Reg, data_size_);		break;
			case 0x8a: MemRegReg(MOV, Reg_MemReg, DataSize::Byte);	break;
			case 0x8b: MemRegReg(MOV, Reg_MemReg, data_size_);		break;
			case 0x8c:
				RequiresMin(i80286);	// TODO: or is this 80386?
				MemRegReg(MOV, MemReg_Seg, DataSize::Word);
			break;
			case 0x8d: MemRegReg(LEA, Reg_MemReg, data_size_);		break;
			case 0x8e: MemRegReg(MOV, Seg_MemReg, DataSize::Word);	break;
			case 0x8f: MemRegReg(POP, MemRegPOP, data_size_);		break;

			case 0x90: Complete(NOP, None, None, DataSize::None);	break;	// Or XCHG AX, AX?
			case 0x91: Complete(XCHG, eAX, eCX, data_size_);		break;
			case 0x92: Complete(XCHG, eAX, eDX, data_size_);		break;
			case 0x93: Complete(XCHG, eAX, eBX, data_size_);		break;
			case 0x94: Complete(XCHG, eAX, eSP, data_size_);		break;
			case 0x95: Complete(XCHG, eAX, eBP, data_size_);		break;
			case 0x96: Complete(XCHG, eAX, eSI, data_size_);		break;
			case 0x97: Complete(XCHG, eAX, eDI, data_size_);		break;

			case 0x98: Complete(CBW, eAX, AH, DataSize::Byte);		break;
			case 0x99: Complete(CWD, eAX, eDX, data_size_);			break;
			case 0x9a: Far(CALLF);									break;
			case 0x9b: Complete(WAIT, None, None, DataSize::None);	break;
			case 0x9c: Complete(PUSHF, None, None, data_size_);		break;
			case 0x9d: Complete(POPF, None, None, data_size_);		break;
			case 0x9e: Complete(SAHF, None, None, DataSize::Byte);	break;
			case 0x9f: Complete(LAHF, None, None, DataSize::Byte);	break;

			case 0xa0: RegAddr(MOV, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xa1: RegAddr(MOV, eAX, data_size_, data_size_);			break;
			case 0xa2: AddrReg(MOV, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xa3: AddrReg(MOV, eAX, data_size_, data_size_);			break;

			case 0xa4: Complete(MOVS, None, None, DataSize::Byte);	break;
			case 0xa5: Complete(MOVS, None, None, data_size_);		break;
			case 0xa6: Complete(CMPS, None, None, DataSize::Byte);	break;
			case 0xa7: Complete(CMPS, None, None, data_size_);		break;
			case 0xa8: RegData(TEST, eAX, DataSize::Byte);			break;
			case 0xa9: RegData(TEST, eAX, data_size_);				break;
			case 0xaa: Complete(STOS, None, None, DataSize::Byte);	break;
			case 0xab: Complete(STOS, None, None, data_size_);		break;
			case 0xac: Complete(LODS, None, None, DataSize::Byte);	break;
			case 0xad: Complete(LODS, None, None, data_size_);		break;
			case 0xae: Complete(SCAS, None, None, DataSize::Byte);	break;
			case 0xaf: Complete(SCAS, None, None, data_size_);		break;

			case 0xb0: RegData(MOV, eAX, DataSize::Byte);	break;
			case 0xb1: RegData(MOV, eCX, DataSize::Byte);	break;
			case 0xb2: RegData(MOV, eDX, DataSize::Byte);	break;
			case 0xb3: RegData(MOV, eBX, DataSize::Byte);	break;
			case 0xb4: RegData(MOV, AH, DataSize::Byte);	break;
			case 0xb5: RegData(MOV, CH, DataSize::Byte);	break;
			case 0xb6: RegData(MOV, DH, DataSize::Byte);	break;
			case 0xb7: RegData(MOV, BH, DataSize::Byte);	break;
			case 0xb8: RegData(MOV, eAX, data_size_);		break;
			case 0xb9: RegData(MOV, eCX, data_size_);		break;
			case 0xba: RegData(MOV, eDX, data_size_);		break;
			case 0xbb: RegData(MOV, eBX, data_size_);		break;
			case 0xbc: RegData(MOV, eSP, data_size_);		break;
			case 0xbd: RegData(MOV, eBP, data_size_);		break;
			case 0xbe: RegData(MOV, eSI, data_size_);		break;
			case 0xbf: RegData(MOV, eDI, data_size_);		break;

			// TODO: 0xc0: shift group 2, Eb, Ib
			// TODO: 0xc1: shift group 2, Ev, Iv

			case 0xc2: RegData(RETN, None, data_size_);				break;
			case 0xc3: Complete(RETN, None, None, DataSize::None);	break;
			case 0xc4: MemRegReg(LES, Reg_MemReg, data_size_);		break;
			case 0xc5: MemRegReg(LDS, Reg_MemReg, data_size_);		break;
			case 0xc6: MemRegReg(MOV, MemRegMOV, DataSize::Byte);	break;
			case 0xc7: MemRegReg(MOV, MemRegMOV, data_size_);		break;

			case 0xc8:
				RequiresMin(i80186);
				Displacement16Operand8(ENTER);
			break;
			case 0xc9:
				RequiresMin(i80186);
				Complete(LEAVE, None, None, DataSize::None);
			break;

			case 0xca: RegData(RETF, None, data_size_);				break;
			case 0xcb: Complete(RETF, None, None, DataSize::DWord);	break;

			case 0xcc: Complete(INT3, None, None, DataSize::None);	break;
			case 0xcd: RegData(INT, None, DataSize::Byte);			break;
			case 0xce: Complete(INTO, None, None, DataSize::None);	break;
			case 0xcf: Complete(IRET, None, None, DataSize::None);	break;

			case 0xd0: case 0xd1: {
				const DataSize sizes[] = {DataSize::Byte, data_size_};
				phase_ = Phase::ModRegRM;
				modregrm_format_ = ModRegRMFormat::MemRegROL_to_SAR;
				operation_size_ = sizes[instr & 1];
				source_ = Source::Immediate;
				operand_ = 1;
			} break;
			case 0xd2: case 0xd3: {
				const DataSize sizes[] = {DataSize::Byte, data_size_};
				phase_ = Phase::ModRegRM;
				modregrm_format_ = ModRegRMFormat::MemRegROL_to_SAR;
				operation_size_ = sizes[instr & 1];
				source_ = Source::eCX;
			} break;
			case 0xd4: RegData(AAM, eAX, DataSize::Byte);			break;
			case 0xd5: RegData(AAD, eAX, DataSize::Byte);			break;
			// Unused: 0xd6.
			case 0xd7: Complete(XLAT, None, None, DataSize::Byte);	break;

			case 0xd8: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;
			case 0xd9: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;
			case 0xda: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;
			case 0xdb: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;
			case 0xdc: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;
			case 0xdd: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;
			case 0xde: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;
			case 0xdf: MemRegReg(ESC, MemReg_Reg, DataSize::None);	break;

			case 0xe0: Jump(LOOPNE, DataSize::Byte);	break;
			case 0xe1: Jump(LOOPE, DataSize::Byte);		break;
			case 0xe2: Jump(LOOP, DataSize::Byte);		break;
			case 0xe3: Jump(JPCX, DataSize::Byte);		break;

			case 0xe4: RegAddr(IN, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe5: RegAddr(IN, eAX, data_size_, DataSize::Byte);		break;
			case 0xe6: AddrReg(OUT, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe7: AddrReg(OUT, eAX, data_size_, DataSize::Byte);		break;

			case 0xe8: RegData(CALLD, None, data_size_);	break;
			case 0xe9: RegData(JMPN, None, data_size_);		break;
			case 0xea: Far(JMPF);							break;
			case 0xeb: Jump(JMPN, DataSize::Byte);			break;

			case 0xec: Complete(IN, eDX, eAX, DataSize::Byte);	break;
			case 0xed: Complete(IN, eDX, eAX, data_size_);		break;
			case 0xee: Complete(OUT, eAX, eDX, DataSize::Byte);	break;
			case 0xef: Complete(OUT, eAX, eDX, data_size_);		break;

			case 0xf0: lock_ = true;					break;
			// Unused: 0xf1
			case 0xf2: repetition_ = Repetition::RepNE;	break;
			case 0xf3: repetition_ = Repetition::RepE;	break;

			case 0xf4: Complete(HLT, None, None, DataSize::None);				break;
			case 0xf5: Complete(CMC, None, None, DataSize::None);				break;
			case 0xf6: MemRegReg(Invalid, MemRegTEST_to_IDIV, DataSize::Byte);	break;
			case 0xf7: MemRegReg(Invalid, MemRegTEST_to_IDIV, data_size_);		break;

			case 0xf8: Complete(CLC, None, None, DataSize::None);	break;
			case 0xf9: Complete(STC, None, None, DataSize::None);	break;
			case 0xfa: Complete(CLI, None, None, DataSize::None);	break;
			case 0xfb: Complete(STI, None, None, DataSize::None);	break;
			case 0xfc: Complete(CLD, None, None, DataSize::None);	break;
			case 0xfd: Complete(STD, None, None, DataSize::None);	break;

			case 0xfe: MemRegReg(Invalid, MemRegINC_DEC, DataSize::Byte);	break;
			case 0xff: MemRegReg(Invalid, MemRegINC_to_PUSH, data_size_);	break;
		}
	}

	// MARK: - Additional F page of instructions.
	if(phase_ == Phase::InstructionPageF && source != end) {
		// Update the instruction acquired.
		const uint8_t instr = *source;
		++source;
		++consumed_;

		// NB: to reach here, the instruction set must be at least
		// that of an 80286.
		switch(instr) {
			default: undefined();

			case 0x00:	MemRegReg(Invalid, MemRegSLDT_to_VERW, data_size_);	break;
			case 0x01:	MemRegReg(Invalid, MemRegSGDT_to_LMSW, data_size_);	break;
			case 0x02:	MemRegReg(LAR, Reg_MemReg, data_size_);				break;
			case 0x03:	MemRegReg(LSL, Reg_MemReg, data_size_);				break;
			case 0x05:
				Requires(i80286);
				Complete(LOADALL, None, None, DataSize::None);
			break;
			case 0x06:	Complete(CLTS, None, None, DataSize::Byte);			break;

			// TODO: 0x20: MOV Cr, Rd
			// TODO: 0x21: MOV Dd, Rd
			// TODO: 0x22: MOV Rd, Cd
			// TODO: 0x23: MOV Rd, Dd
			// TODO: 0x24: MOV Td, Rd
			// TODO: 0x26: MOV Rd, Td

			case 0x70: RequiresMin(i80386);	Jump(JO, data_size_);	break;
			case 0x71: RequiresMin(i80386);	Jump(JNO, data_size_);	break;
			case 0x72: RequiresMin(i80386);	Jump(JB, data_size_);	break;
			case 0x73: RequiresMin(i80386);	Jump(JNB, data_size_);	break;
			case 0x74: RequiresMin(i80386);	Jump(JE, data_size_);	break;
			case 0x75: RequiresMin(i80386);	Jump(JNE, data_size_);	break;
			case 0x76: RequiresMin(i80386);	Jump(JBE, data_size_);	break;
			case 0x77: RequiresMin(i80386);	Jump(JNBE, data_size_);	break;
			case 0x78: RequiresMin(i80386);	Jump(JS, data_size_);	break;
			case 0x79: RequiresMin(i80386);	Jump(JNS, data_size_);	break;
			case 0x7a: RequiresMin(i80386);	Jump(JP, data_size_);	break;
			case 0x7b: RequiresMin(i80386);	Jump(JNP, data_size_);	break;
			case 0x7c: RequiresMin(i80386);	Jump(JL, data_size_);	break;
			case 0x7d: RequiresMin(i80386);	Jump(JNL, data_size_);	break;
			case 0x7e: RequiresMin(i80386);	Jump(JLE, data_size_);	break;
			case 0x7f: RequiresMin(i80386);	Jump(JNLE, data_size_);	break;

			// TODO: [0x90, 0x97]: byte set on condition Eb: SETO, SETNO, SETB, SETNB, SETZ, SETNZ, SETBE, SETNBE
			// TODO: [0x98, 0x9f]: SETS, SETNS, SETP, SETNP, SETL, SETNL, SETLE, SETNLE

			case 0xa0:	RequiresMin(i80386);	Complete(PUSH, FS, None, data_size_);	break;
			case 0xa1:	RequiresMin(i80386);	Complete(POP, FS, None, data_size_);	break;
			// TODO: 0xa3: BT Ev, Gv
			// TODO: 0xa4: SHLD EvGvIb
			// TODO: 0xa5: SHLD EvGcCL
			case 0xa8:	RequiresMin(i80386);	Complete(PUSH, GS, None, data_size_);	break;
			case 0xa9:	RequiresMin(i80386);	Complete(POP, GS, None, data_size_);	break;
			// TODO: 0xab: BTS Ev, Gv
			// TODO: 0xac: SHRD EvGvIb
			// TODO: 0xad: SHRD EvGvCL
			// TODO: 0xaf: IMUL Gv, Ev

			// TODO: 0xb2: LSS Mp
			// TODO: 0xb3: BTR Ev, Gv
			// TODO: 0xb4: LFS Mp
			// TODO: 0xb5: LGS Mp
			// TODO: 0xb6: MOVZX Gv, Eb
			// TODO: 0xb7: MOVZX Gv, Ew
			// TODO: 0xba: Grp8 Ev, Ib
			// TODO: 0xbb: BTC Ev, Gv
			// TODO: 0xbc: BSF Gv, Ev
			// TODO: 0xbd: BSR Gv, Ev
			// TODO: 0xbe: MOVSX Gv, Eb
			// TODO: 0xbf: MOVSX Gv, Ew
		}
	}

#undef Requires
#undef RequiresMin
#undef Far
#undef Jump
#undef MemRegReg
#undef AddrReg
#undef RegAddr
#undef RegData
#undef Complete
#undef SetOpSrcDestSize

	// MARK: - ModRegRM byte, if any.

	if(phase_ == Phase::ModRegRM && source != end) {
		const uint8_t mod = *source >> 6;		// i.e. mode.
		const uint8_t reg = (*source >> 3) & 7;	// i.e. register.
		const uint8_t rm = *source & 7;			// i.e. register/memory.
		++source;
		++consumed_;

		Source memreg;

		// TODO: the below currently has no way to segue into fetching a SIB.

		// TODO: can I just eliminate these lookup tables given the deliberate ordering within Source?
		constexpr Source reg_table[8] = {
			Source::eAX,		Source::eCX,		Source::eDX,		Source::eBX,
			Source::eSPorAH,	Source::eBPorCH,	Source::eSIorDH,	Source::eDIorBH,
		};
		constexpr Source seg_table[6] = {
			Source::ES,	Source::CS,	Source::SS,	Source::DS,	Source::FS,	Source::GS
		};
		switch(mod) {
			default: {
				const DataSize sizes[] = {DataSize::Byte, data_size_};
				displacement_size_ = sizes[mod == 2];
			}
				[[fallthrough]];
			case 0: {
				constexpr ScaleIndexBase rm_table[8] = {
					ScaleIndexBase(0, Source::eBX, Source::eSI),
					ScaleIndexBase(0, Source::eBX, Source::eDI),
					ScaleIndexBase(0, Source::eBP, Source::eSI),
					ScaleIndexBase(0, Source::eBP, Source::eDI),
					ScaleIndexBase(0, Source::None, Source::eSI),
					ScaleIndexBase(0, Source::None, Source::eDI),
					ScaleIndexBase(0, Source::None, Source::eBP),
					ScaleIndexBase(0, Source::None, Source::eBX),
				};

				memreg = Source::Indirect;
				sib_ = rm_table[rm];
			} break;

			// Other operand is just a register.
			case 3:
				memreg = reg_table[rm];

				// LES and LDS accept a memory argument only, not a register.
				if(operation_ == Operation::LES || operation_ == Operation::LDS) {
					undefined();
				}
			break;
		}

		switch(modregrm_format_) {
			case ModRegRMFormat::Reg_MemReg:
			case ModRegRMFormat::MemReg_Reg: {
				if(modregrm_format_ == ModRegRMFormat::Reg_MemReg) {
					source_ = memreg;
					destination_ = reg_table[reg];
				} else {
					source_ = reg_table[reg];
					destination_ = memreg;
				}
			} break;

			case ModRegRMFormat::MemRegTEST_to_IDIV:
				source_ = destination_ = memreg;

				switch(reg) {
					default: undefined();

					case 0: 	operation_ = Operation::TEST;	break;
					case 2: 	operation_ = Operation::NOT;	break;
					case 3: 	operation_ = Operation::NEG;	break;
					case 4: 	operation_ = Operation::MUL;	break;
					case 5: 	operation_ = Operation::IMUL;	break;
					case 6: 	operation_ = Operation::DIV;	break;
					case 7: 	operation_ = Operation::IDIV;	break;
				}
			break;

			case ModRegRMFormat::Seg_MemReg:
			case ModRegRMFormat::MemReg_Seg:
				// The 16-bit chips have four segment registers;
				// the 80386 onwards has six.
				if(!is_32bit(model) && reg > 3) {
					undefined();
				} else if(reg > 5) {
					undefined();
				}

				if(modregrm_format_ == ModRegRMFormat::Seg_MemReg) {
					source_ = memreg;
					destination_ = seg_table[reg];

					// 80286 and later disallow MOV to CS.
					if(model >= Model::i80286 && destination_ == Source::CS) {
						undefined();
					}
				} else {
					source_ = seg_table[reg];
					destination_ = memreg;
				}
			break;

			case ModRegRMFormat::MemRegROL_to_SAR:
				destination_ = memreg;

				switch(reg) {
					default: 	undefined();

					case 0: 	operation_ = Operation::ROL;	break;
					case 2: 	operation_ = Operation::ROR;	break;
					case 3: 	operation_ = Operation::RCL;	break;
					case 4: 	operation_ = Operation::RCR;	break;
					case 5: 	operation_ = Operation::SAL;	break;
					case 6: 	operation_ = Operation::SHR;	break;
					case 7: 	operation_ = Operation::SAR;	break;
				}
			break;

			case ModRegRMFormat::MemRegINC_DEC:
				source_ = destination_ = memreg;

				switch(reg) {
					default: 	undefined();

					case 0:		operation_ = Operation::INC;	break;
					case 1:		operation_ = Operation::DEC;	break;
				}
			break;

			case ModRegRMFormat::MemRegINC_to_PUSH:
				source_ = destination_ = memreg;

				switch(reg) {
					default: 	undefined();

					case 0:		operation_ = Operation::INC;	break;
					case 1:		operation_ = Operation::DEC;	break;
					case 2:		operation_ = Operation::CALLN;	break;
					case 3:
						operation_ = Operation::CALLF;
						operand_size_ = DataSize::DWord;
						source_ = Source::Immediate;
					break;
					case 4:		operation_ = Operation::JMPN;	break;
					case 5:
						operation_ = Operation::JMPF;
						operand_size_ = DataSize::DWord;
						source_ = Source::Immediate;
					break;
					case 6:	operation_ = Operation::PUSH;		break;
				}
			break;

			case ModRegRMFormat::MemRegPOP:
				source_ = destination_ = memreg;

				if(reg != 0) {
					undefined();
				}
			break;

			case ModRegRMFormat::MemRegMOV:
				source_ = Source::Immediate;
				destination_ = memreg;
				operand_size_ = operation_size_;
			break;

			case ModRegRMFormat::MemRegADD_to_CMP:
			case ModRegRMFormat::MemRegADD_to_CMP_SignExtend:
				source_ = Source::Immediate;
				destination_ = memreg;
				operand_size_ = (modregrm_format_ == ModRegRMFormat::MemRegADD_to_CMP_SignExtend) ? DataSize::Byte : operation_size_;
				sign_extend_ = true;	// Will be effective only if modregrm_format_ == ModRegRMFormat::MemRegADD_to_CMP_SignExtend.

				switch(reg) {
					default:	operation_ = Operation::ADD;	break;
					case 1:		operation_ = Operation::OR;		break;
					case 2:		operation_ = Operation::ADC;	break;
					case 3:		operation_ = Operation::SBB;	break;
					case 4:		operation_ = Operation::AND;	break;
					case 5:		operation_ = Operation::SUB;	break;
					case 6:		operation_ = Operation::XOR;	break;
					case 7:		operation_ = Operation::CMP;	break;
				}
			break;

			case ModRegRMFormat::MemRegSLDT_to_VERW:
				destination_ = source_ = memreg;

				switch(reg) {
					default: undefined();

					case 0: 	operation_ = Operation::SLDT;	break;
					case 1: 	operation_ = Operation::STR;	break;
					case 2: 	operation_ = Operation::LLDT;	break;
					case 3: 	operation_ = Operation::LTR;	break;
					case 4: 	operation_ = Operation::VERR;	break;
					case 5: 	operation_ = Operation::VERW;	break;
				}
			break;

			case ModRegRMFormat::MemRegSGDT_to_LMSW:
				destination_ = source_ = memreg;

				switch(reg) {
					default: undefined();

					case 0: 	operation_ = Operation::SGDT;	break;
					case 2: 	operation_ = Operation::LGDT;	break;
					case 4: 	operation_ = Operation::SMSW;	break;
					case 6: 	operation_ = Operation::LMSW;	break;
				}
			break;

			default: assert(false);
		}

		phase_ = (displacement_size_ != DataSize::None || operand_size_ != DataSize::None) ? Phase::DisplacementOrOperand : Phase::ReadyToPost;
	}

#undef undefined

	// MARK: - ScaleIndexBase

	if(phase_ == Phase::ScaleIndexBase && source != end) {
		sib_ = *source;
		++source;
		++consumed_;
	}

	// MARK: - Displacement and operand.

	if(phase_ == Phase::DisplacementOrOperand && source != end) {
		const auto required_bytes = int(byte_size(displacement_size_) + byte_size(operand_size_));

		const int outstanding_bytes = required_bytes - operand_bytes_;
		const int bytes_to_consume = std::min(int(end - source), outstanding_bytes);

		for(int c = 0; c < bytes_to_consume; c++) {
			inward_data_ |= decltype(inward_data_)(source[0]) << next_inward_data_shift_;
			++source;
			next_inward_data_shift_ += 8;
		}

		consumed_ += bytes_to_consume;
		operand_bytes_ += bytes_to_consume;

		if(bytes_to_consume == outstanding_bytes) {
			phase_ = Phase::ReadyToPost;

			switch(displacement_size_) {
				case DataSize::None:	displacement_ = 0;						break;
				case DataSize::Byte:	displacement_ = int8_t(inward_data_);	break;
				case DataSize::Word:	displacement_ = int16_t(inward_data_);	break;
				case DataSize::DWord:	displacement_ = int32_t(inward_data_);	break;
			}
			inward_data_ >>= bit_size(displacement_size_);

			// Use inequality of sizes as a test for necessary sign extension.
			if(operand_size_ == data_size_ || !sign_extend_) {
				operand_ = decltype(operand_)(inward_data_);
			} else {
				switch(operand_size_) {
					case DataSize::None:	operand_ = 0;											break;
					case DataSize::Byte:	operand_ = decltype(operand_)(int8_t(inward_data_));	break;
					case DataSize::Word:	operand_ = decltype(operand_)(int16_t(inward_data_));	break;
					case DataSize::DWord:	operand_ = decltype(operand_)(int32_t(inward_data_));	break;
				}
			}

			// TODO: split differently for far jumps/etc. But that information is
			// no longer retained now that it's not implied by a DWord-sized operand.
		} else {
			// Provide a genuine measure of further bytes required.
			return std::make_pair(-(outstanding_bytes - bytes_to_consume), InstructionT());
		}
	}

	// MARK: - Check for completion.

	if(phase_ == Phase::ReadyToPost) {
		const auto result = std::make_pair(
			consumed_,
			InstructionT(
				operation_,
				source_,
				destination_,
				sib_,
				lock_,
				address_size_,
				segment_override_,
				repetition_,
				DataSize(operation_size_),
				static_cast<typename InstructionT::DisplacementT>(displacement_),
				static_cast<typename InstructionT::ImmediateT>(operand_))
		);
		reset_parsing();
		return result;
	}

	// i.e. not done yet.
	return std::make_pair(0, InstructionT());
}

template <Model model> void Decoder<model>::set_32bit_protected_mode(bool enabled) {
	if constexpr (!is_32bit(model)) {
		assert(!enabled);
		return;
	}

	if(enabled) {
		default_address_size_ = address_size_ = AddressSize::b32;
		default_data_size_ = data_size_ = DataSize::DWord;
	} else {
		default_address_size_ = address_size_ = AddressSize::b16;
		default_data_size_ = data_size_ = DataSize::Word;
	}
}

// Ensure all possible decoders are built.
template class InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086>;
template class InstructionSet::x86::Decoder<InstructionSet::x86::Model::i80186>;
template class InstructionSet::x86::Decoder<InstructionSet::x86::Model::i80286>;
template class InstructionSet::x86::Decoder<InstructionSet::x86::Model::i80386>;
