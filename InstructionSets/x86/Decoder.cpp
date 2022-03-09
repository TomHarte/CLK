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

/// Handles JO, JNO, JB, etc — anything with only a displacement.
#define Displacement(op, size)						\
	operation_ = Operation::op;						\
	phase_ = Phase::DisplacementOrOperand;			\
	displacement_size_ = size

/// Handles PUSH [immediate], etc — anything with only an immediate operand.
#define Immediate(op, size)							\
	operation_ = Operation::op;						\
	source_ = Source::Immediate;					\
	phase_ = Phase::DisplacementOrOperand;			\
	operand_size_ = size

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

/// Sets up the operation size, oncoming phase and modregrm format for a member of the shift group (i.e. 'group 2').
#define ShiftGroup() {										\
	const DataSize sizes[] = {DataSize::Byte, data_size_};	\
	phase_ = Phase::ModRegRM;								\
	modregrm_format_ = ModRegRMFormat::MemRegROL_to_SAR;	\
	operation_size_ = sizes[instr & 1];						\
}

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
			case 0x68:
				RequiresMin(i80286);
				Immediate(PUSH, data_size_);
				operation_size_ = data_size_;
			break;
			case 0x69:
				RequiresMin(i80286);
				MemRegReg(IMUL_3, Reg_MemReg, data_size_);
				operand_size_ = data_size_;
			break;
			case 0x6a:
				RequiresMin(i80286);
				Immediate(PUSH, DataSize::Byte);
			break;
			case 0x6b:
				RequiresMin(i80286);
				MemRegReg(IMUL_3, Reg_MemReg, data_size_);
				operand_size_ = DataSize::Byte;
				sign_extend_ = true;
			break;
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

			case 0x70: Displacement(JO, DataSize::Byte);	break;
			case 0x71: Displacement(JNO, DataSize::Byte);	break;
			case 0x72: Displacement(JB, DataSize::Byte);	break;
			case 0x73: Displacement(JNB, DataSize::Byte);	break;
			case 0x74: Displacement(JE, DataSize::Byte);	break;
			case 0x75: Displacement(JNE, DataSize::Byte);	break;
			case 0x76: Displacement(JBE, DataSize::Byte);	break;
			case 0x77: Displacement(JNBE, DataSize::Byte);	break;
			case 0x78: Displacement(JS, DataSize::Byte);	break;
			case 0x79: Displacement(JNS, DataSize::Byte);	break;
			case 0x7a: Displacement(JP, DataSize::Byte);	break;
			case 0x7b: Displacement(JNP, DataSize::Byte);	break;
			case 0x7c: Displacement(JL, DataSize::Byte);	break;
			case 0x7d: Displacement(JNL, DataSize::Byte);	break;
			case 0x7e: Displacement(JLE, DataSize::Byte);	break;
			case 0x7f: Displacement(JNLE, DataSize::Byte);	break;

			case 0x80: MemRegReg(Invalid, MemRegADD_to_CMP, DataSize::Byte);			break;
			case 0x81: MemRegReg(Invalid, MemRegADD_to_CMP, data_size_);				break;
			case 0x82: MemRegReg(Invalid, MemRegADD_to_CMP_SignExtend, DataSize::Byte);	break;
			case 0x83: MemRegReg(Invalid, MemRegADD_to_CMP_SignExtend, data_size_);		break;

			case 0x84: MemRegReg(TEST, MemReg_Reg, DataSize::Byte);		break;
			case 0x85: MemRegReg(TEST, MemReg_Reg, data_size_);			break;
			case 0x86: MemRegReg(XCHG, Reg_MemReg, DataSize::Byte);		break;
			case 0x87: MemRegReg(XCHG, Reg_MemReg, data_size_);			break;
			case 0x88: MemRegReg(MOV, MemReg_Reg, DataSize::Byte);		break;
			case 0x89: MemRegReg(MOV, MemReg_Reg, data_size_);			break;
			case 0x8a: MemRegReg(MOV, Reg_MemReg, DataSize::Byte);		break;
			case 0x8b: MemRegReg(MOV, Reg_MemReg, data_size_);			break;
			case 0x8c: MemRegReg(MOV, MemReg_Seg, DataSize::Word);		break;
			case 0x8d: MemRegReg(LEA, Reg_MemReg, data_size_);			break;
			case 0x8e: MemRegReg(MOV, Seg_MemReg, DataSize::Word);		break;
			case 0x8f: MemRegReg(POP, MemRegSingleOperand, data_size_);	break;

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

			case 0xa0: RegAddr(MOV, eAX, DataSize::Byte, data_size(address_size_));	break;
			case 0xa1: RegAddr(MOV, eAX, data_size_, data_size(address_size_));		break;
			case 0xa2: AddrReg(MOV, eAX, DataSize::Byte, data_size(address_size_));	break;
			case 0xa3: AddrReg(MOV, eAX, data_size_, data_size(address_size_));		break;

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

			case 0xc0: case 0xc1:
				RequiresMin(i80186);
				ShiftGroup();
				source_ = Source::Immediate;
				operand_size_ = DataSize::Byte;
			break;
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

			case 0xd0: case 0xd1:
				ShiftGroup();
				source_ = Source::Immediate;
				operand_ = 1;
			break;
			case 0xd2: case 0xd3:
				ShiftGroup();
				source_ = Source::eCX;
			break;
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

			case 0xe0: Displacement(LOOPNE, DataSize::Byte);	break;
			case 0xe1: Displacement(LOOPE, DataSize::Byte);		break;
			case 0xe2: Displacement(LOOP, DataSize::Byte);		break;
			case 0xe3: Displacement(JPCX, DataSize::Byte);		break;

			case 0xe4: RegAddr(IN, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe5: RegAddr(IN, eAX, data_size_, DataSize::Byte);		break;
			case 0xe6: AddrReg(OUT, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe7: AddrReg(OUT, eAX, data_size_, DataSize::Byte);		break;

			case 0xe8: RegData(CALLD, None, data_size_);	break;
			case 0xe9: RegData(JMPN, None, data_size_);		break;
			case 0xea: Far(JMPF);							break;
			case 0xeb: Displacement(JMPN, DataSize::Byte);	break;

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

			case 0x20:
				RequiresMin(i80386);
				MemRegReg(MOVfromCr, Reg_MemReg, DataSize::DWord);
			break;
			case 0x21:
				RequiresMin(i80386);
				MemRegReg(MOVfromDr, Reg_MemReg, DataSize::DWord);
			break;
			case 0x22:
				RequiresMin(i80386);
				MemRegReg(MOVtoCr, Reg_MemReg, DataSize::DWord);
			break;
			case 0x23:
				RequiresMin(i80386);
				MemRegReg(MOVtoDr, Reg_MemReg, DataSize::DWord);
			break;
			case 0x24:
				RequiresMin(i80386);
				MemRegReg(MOVfromTr, Reg_MemReg, DataSize::DWord);
			break;
			case 0x26:
				RequiresMin(i80386);
				MemRegReg(MOVtoTr, Reg_MemReg, DataSize::DWord);
			break;

			case 0x70: RequiresMin(i80386);	Displacement(JO, data_size_);	break;
			case 0x71: RequiresMin(i80386);	Displacement(JNO, data_size_);	break;
			case 0x72: RequiresMin(i80386);	Displacement(JB, data_size_);	break;
			case 0x73: RequiresMin(i80386);	Displacement(JNB, data_size_);	break;
			case 0x74: RequiresMin(i80386);	Displacement(JE, data_size_);	break;
			case 0x75: RequiresMin(i80386);	Displacement(JNE, data_size_);	break;
			case 0x76: RequiresMin(i80386);	Displacement(JBE, data_size_);	break;
			case 0x77: RequiresMin(i80386);	Displacement(JNBE, data_size_);	break;
			case 0x78: RequiresMin(i80386);	Displacement(JS, data_size_);	break;
			case 0x79: RequiresMin(i80386);	Displacement(JNS, data_size_);	break;
			case 0x7a: RequiresMin(i80386);	Displacement(JP, data_size_);	break;
			case 0x7b: RequiresMin(i80386);	Displacement(JNP, data_size_);	break;
			case 0x7c: RequiresMin(i80386);	Displacement(JL, data_size_);	break;
			case 0x7d: RequiresMin(i80386);	Displacement(JNL, data_size_);	break;
			case 0x7e: RequiresMin(i80386);	Displacement(JLE, data_size_);	break;
			case 0x7f: RequiresMin(i80386);	Displacement(JNLE, data_size_);	break;

#define Set(x)												\
	RequiresMin(i80386);									\
	MemRegReg(SET##x, MemRegSingleOperand, DataSize::Byte);

			case 0x90: Set(O);		break;
			case 0x91: Set(NO);		break;
			case 0x92: Set(B);		break;
			case 0x93: Set(NB);		break;
			case 0x94: Set(Z);		break;
			case 0x95: Set(NZ);		break;
			case 0x96: Set(BE);		break;
			case 0x97: Set(NBE);	break;
			case 0x98: Set(S);		break;
			case 0x99: Set(NS);		break;
			case 0x9a: Set(P);		break;
			case 0x9b: Set(NP);		break;
			case 0x9c: Set(L);		break;
			case 0x9d: Set(NL);		break;
			case 0x9e: Set(LE);		break;
			case 0x9f: Set(NLE);	break;

#undef Set

			case 0xa0: RequiresMin(i80386);	Complete(PUSH, FS, None, data_size_);	break;
			case 0xa1: RequiresMin(i80386);	Complete(POP, FS, None, data_size_);	break;
			case 0xa3: RequiresMin(i80386);	MemRegReg(BT, MemReg_Reg, data_size_);	break;
			case 0xa4:
				RequiresMin(i80386);
				MemRegReg(SHLDimm, Reg_MemReg, data_size_);
				operand_size_ = DataSize::Byte;
			break;
			case 0xa5:
				RequiresMin(i80386);
				MemRegReg(SHLDCL, MemReg_Reg, data_size_);
			break;
			case 0xa8: RequiresMin(i80386);	Complete(PUSH, GS, None, data_size_);	break;
			case 0xa9: RequiresMin(i80386);	Complete(POP, GS, None, data_size_);	break;
			case 0xab: RequiresMin(i80386);	MemRegReg(BTS, MemReg_Reg, data_size_);	break;
			case 0xac:
				RequiresMin(i80386);
				MemRegReg(SHRDimm, Reg_MemReg, data_size_);
				operand_size_ = DataSize::Byte;
			break;
			case 0xad:
				RequiresMin(i80386);
				MemRegReg(SHRDCL, MemReg_Reg, data_size_);
			break;
			case 0xaf:
				RequiresMin(i80386);
				MemRegReg(IMUL_2, Reg_MemReg, data_size_);
			break;

			case 0xb2: RequiresMin(i80386);	MemRegReg(LSS, Reg_MemReg, data_size_);	break;
			case 0xb3: RequiresMin(i80386);	MemRegReg(BTR, MemReg_Reg, data_size_);	break;
			case 0xb4: RequiresMin(i80386);	MemRegReg(LFS, Reg_MemReg, data_size_);	break;
			case 0xb5: RequiresMin(i80386);	MemRegReg(LGS, Reg_MemReg, data_size_);	break;
			case 0xb6:
				RequiresMin(i80386);
				MemRegReg(MOVZX, Reg_MemReg, DataSize::Byte);
			break;
			case 0xb7:
				RequiresMin(i80386);
				MemRegReg(MOVZX, Reg_MemReg, DataSize::Word);
			break;
			case 0xba: RequiresMin(i80386);	MemRegReg(Invalid, MemRegBT_to_BTC, data_size_);	break;
			case 0xbb: RequiresMin(i80386);	MemRegReg(BTC, MemReg_Reg, data_size_);				break;
			case 0xbc: RequiresMin(i80386);	MemRegReg(BSF, MemReg_Reg, data_size_);				break;
			case 0xbd: RequiresMin(i80386);	MemRegReg(BSR, MemReg_Reg, data_size_);				break;
			case 0xbe:
				RequiresMin(i80386);
				MemRegReg(MOVSX, Reg_MemReg, DataSize::Byte);
			break;
			case 0xbf:
				RequiresMin(i80386);
				MemRegReg(MOVSX, Reg_MemReg, DataSize::Word);
			break;
		}
	}

#undef Requires
#undef RequiresMin
#undef ShiftGroup
#undef Displacement16Operand8
#undef Far
#undef Immediate
#undef Displacement
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
		bool expects_sib = false;
		++source;
		++consumed_;

		Source memreg;

		// TODO: can I just eliminate these lookup tables given the deliberate ordering within Source?
		constexpr Source reg_table[8] = {
			Source::eAX,		Source::eCX,		Source::eDX,		Source::eBX,
			Source::eSPorAH,	Source::eBPorCH,	Source::eSIorDH,	Source::eDIorBH,
		};
		constexpr Source seg_table[6] = {
			Source::ES,	Source::CS,	Source::SS,	Source::DS,	Source::FS,	Source::GS
		};

		// Mode 3 is the same regardless of 16/32-bit mode. So deal with that up front.
		if(mod == 3) {
			// Other operand is just a register.
			memreg = reg_table[rm];

			// LES, LDS, etc accept a memory argument only, not a register.
			if(
				operation_ == Operation::LES ||
				operation_ == Operation::LDS ||
				operation_ == Operation::LGS ||
				operation_ == Operation::LSS ||
				operation_ == Operation::LFS) {
				undefined();
			}
		} else {
			const DataSize sizes[] = {
				DataSize::None,
				DataSize::Byte,
				data_size(address_size_)
			};
			displacement_size_ = sizes[mod];
			memreg = Source::Indirect;

			if(address_size_ == AddressSize::b32) {
				// 32-bit decoding: the range of potential indirections is expanded,
				// and may segue into obtaining a SIB.
				sib_ = ScaleIndexBase(0, Source::None, reg_table[rm]);
				expects_sib = rm == 4;	// Indirect via eSP isn't directly supported; it's the
										// escape indicator for reading a SIB.
			} else {
				// Classic 16-bit decoding: mode picks a displacement size,
				// and a few fixed index+base pairs are defined.
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

				sib_ = rm_table[rm];
			}
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
					case 5: 	operation_ = Operation::IMUL_1;	break;
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
					case 1: 	operation_ = Operation::ROR;	break;
					case 2: 	operation_ = Operation::RCL;	break;
					case 3: 	operation_ = Operation::RCR;	break;
					case 4: 	operation_ = Operation::SAL;	break;
					case 5: 	operation_ = Operation::SHR;	break;
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

			case ModRegRMFormat::MemRegSingleOperand:
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

			case ModRegRMFormat::MemRegBT_to_BTC:
				destination_ = memreg;
				source_ = Source::Immediate;
				operand_size_ = DataSize::Byte;

				switch(reg) {
					default:	undefined();

					case 4:		operation_ = Operation::BT;		break;
					case 5:		operation_ = Operation::BTS;	break;
					case 6:		operation_ = Operation::BTR;	break;
					case 7:		operation_ = Operation::BTC;	break;
				}
			break;

			default: assert(false);
		}

		if(expects_sib && (source_ == Source::Indirect | destination_ == Source::Indirect)) {
			phase_ = Phase::ScaleIndexBase;
		} else {
			phase_ = (displacement_size_ != DataSize::None || operand_size_ != DataSize::None) ? Phase::DisplacementOrOperand : Phase::ReadyToPost;
		}
	}

#undef undefined

	// MARK: - ScaleIndexBase

	if(phase_ == Phase::ScaleIndexBase && source != end) {
		sib_ = *source;
		++source;
		++consumed_;

		phase_ = (displacement_size_ != DataSize::None || operand_size_ != DataSize::None) ? Phase::DisplacementOrOperand : Phase::ReadyToPost;
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
