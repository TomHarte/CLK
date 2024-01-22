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
std::pair<int, typename Decoder<model>::InstructionT> Decoder<model>::decode(const uint8_t *source, std::size_t length) {
	// Instruction length limits:
	//
	//	8086/80186: none*
	//	80286: 10 bytes
	//	80386: 15 bytes
	//
	// * but, can treat internally as a limit of 65536 bytes — after that distance the IP will
	// be back to wherever it started, so it's safe to spit out a NOP and reset parsing
	// without any loss of context. This reduces the risk of the decoder tricking a caller into
	// an infinite loop.
	static constexpr int max_instruction_length = model >= Model::i80386 ? 15 : (model == Model::i80286 ? 10 : 65536);
	const uint8_t *const end = source + std::min(length, size_t(max_instruction_length - consumed_));

	// MARK: - Prefixes (if present) and the opcode.

/// Sets the operation and verifies that the current repetition, if any, is compatible, discarding it otherwise.
#define SetOperation(op)	\
	operation_ = rep_operation<model>(op, repetition_);

/// Helper macro for those that follow.
#define SetOpSrcDestSize(op, src, dest, size)	\
	SetOperation(Operation::op);				\
	source_ = Source::src;						\
	destination_ = Source::dest;				\
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
	displacement_size_ = addr_size;						\
	phase_ = Phase::DisplacementOrOperand;				\
	sign_extend_displacement_ = false

/// Handles instructions of the form jjkk, Ax where the former is implicitly an address.
#define AddrReg(op, source, op_size, addr_size)				\
	SetOpSrcDestSize(op, source, DirectAddress, op_size);	\
	displacement_size_ = addr_size;							\
	destination_ = Source::DirectAddress;					\
	phase_ = Phase::DisplacementOrOperand;					\
	sign_extend_displacement_ = false

/// Covers both `mem/reg, reg` and `reg, mem/reg`.
#define MemRegReg(op, format, size)				\
	SetOperation(Operation::op);				\
	phase_ = Phase::ModRegRM;					\
	modregrm_format_ = ModRegRMFormat::format;	\
	operand_size_ = DataSize::None;				\
	operation_size_ = size

/// Handles JO, JNO, JB, etc — anything with only a displacement.
#define Displacement(op, size)						\
	SetOperation(Operation::op);					\
	phase_ = Phase::DisplacementOrOperand;			\
	operation_size_= displacement_size_ = size

/// Handles PUSH [immediate], etc — anything with only an immediate operand.
#define Immediate(op, size)							\
	SetOperation(Operation::op);					\
	source_ = Source::Immediate;					\
	phase_ = Phase::DisplacementOrOperand;			\
	operand_size_ = size

/// Handles far CALL and far JMP — fixed four or six byte operand operations.
#define Far(op)											\
	SetOperation(Operation::op);						\
	phase_ = Phase::DisplacementOrOperand;				\
	operation_size_ = operand_size_ = DataSize::Word;	\
	destination_ = Source::Immediate;					\
	displacement_size_ = data_size(default_address_size_)

/// Handles ENTER — a fixed three-byte operation.
#define Displacement16Operand8(op)					\
	SetOperation(Operation::op);					\
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
				if constexpr (model < Model::i80286) {
					Complete(POP, None, CS, data_size_);
				} else {
					phase_ = Phase::InstructionPageF;
				}
			break;

			PartialBlock(0x10, ADC);								break;
			case 0x16: Complete(PUSH, SS, None, DataSize::Word);	break;
			case 0x17: Complete(POP, None, SS, DataSize::Word);		break;

			PartialBlock(0x18, SBB);								break;
			case 0x1e: Complete(PUSH, DS, None, DataSize::Word);	break;
			case 0x1f: Complete(POP, None, DS, DataSize::Word);		break;

			PartialBlock(0x20, AND);								break;
			case 0x26: segment_override_ = Source::ES;				break;
			case 0x27: Complete(DAA, None, None, DataSize::Byte);	break;

			PartialBlock(0x28, SUB);								break;
			case 0x2e: segment_override_ = Source::CS;				break;
			case 0x2f: Complete(DAS, None, None, DataSize::Byte);	break;

			PartialBlock(0x30, XOR);								break;
			case 0x36: segment_override_ = Source::SS;				break;
			case 0x37: Complete(AAA, None, None, DataSize::Word);	break;

			PartialBlock(0x38, CMP);								break;
			case 0x3e: segment_override_ = Source::DS;				break;
			case 0x3f: Complete(AAS, None, None, DataSize::Word);	break;

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
				if constexpr (model < Model::i80186) {
					Displacement(JO, DataSize::Byte);
				} else {
					Complete(PUSHA, None, None, data_size_);
				}
			break;
			case 0x61:
				if constexpr (model < Model::i80186) {
					Displacement(JNO, DataSize::Byte);
				} else {
					Complete(POPA, None, None, data_size_);
				}
			break;
			case 0x62:
				if constexpr (model < Model::i80186) {
					Displacement(JB, DataSize::Byte);
				} else {
					MemRegReg(BOUND, Reg_MemReg, data_size_);
				}
			break;
			case 0x63:
				if constexpr (model < Model::i80286) {
					Displacement(JNB, DataSize::Byte);
				} else {
					MemRegReg(ARPL, MemReg_Reg, DataSize::Word);
				}
			break;
			case 0x64:
				if constexpr (model < Model::i80386) {
					Displacement(JZ, DataSize::Byte);
				} else {
					RequiresMin(i80386);
					segment_override_ = Source::FS;
				}
			break;
			case 0x65:
				if constexpr (model < Model::i80286) {
					Displacement(JNZ, DataSize::Byte);
					break;
				}
				RequiresMin(i80386);
				segment_override_ = Source::GS;
			break;
			case 0x66:
				if constexpr (model < Model::i80286) {
					Displacement(JBE, DataSize::Byte);
					break;
				}
				RequiresMin(i80386);
				data_size_ = DataSize(int(default_data_size_) ^ int(DataSize::Word) ^ int(DataSize::DWord));
			break;
			case 0x67:
				if constexpr (model < Model::i80286) {
					Displacement(JNBE, DataSize::Byte);
					break;
				}
				RequiresMin(i80386);
				address_size_ = AddressSize(int(default_address_size_) ^ int(AddressSize::b16) ^ int(AddressSize::b32));
			break;
			case 0x68:
				if constexpr (model < Model::i80286) {
					Displacement(JS, DataSize::Byte);
				} else {
					Immediate(PUSH, data_size_);
					operation_size_ = data_size_;
				}
			break;
			case 0x69:
				if constexpr (model < Model::i80286) {
					Displacement(JNS, DataSize::Byte);
				} else {
					MemRegReg(IMUL_3, Reg_MemReg, data_size_);
					operand_size_ = data_size_;
				}
			break;
			case 0x6a:
				if constexpr (model < Model::i80286) {
					Displacement(JP, DataSize::Byte);
				} else {
					Immediate(PUSH, DataSize::Byte);
				}
			break;
			case 0x6b:
				if constexpr (model < Model::i80286) {
					Displacement(JNP, DataSize::Byte);
				} else {
					MemRegReg(IMUL_3, Reg_MemReg, data_size_);
					operand_size_ = DataSize::Byte;
					sign_extend_operand_ = true;
				}
			break;
			case 0x6c:	// INSB
				if constexpr (model < Model::i80186) {
					Displacement(JL, DataSize::Byte);
				} else {
					Complete(INS, None, None, DataSize::Byte);
				}
			break;
			case 0x6d:	// INSW/INSD
				if constexpr (model < Model::i80186) {
					Displacement(JNL, DataSize::Byte);
				} else {
					Complete(INS, None, None, data_size_);
				}
			break;
			case 0x6e:	// OUTSB
				if constexpr (model < Model::i80186) {
					Displacement(JLE, DataSize::Byte);
				} else {
					Complete(OUTS, None, None, DataSize::Byte);
				}
			break;
			case 0x6f:	// OUTSW/OUSD
				if constexpr (model < Model::i80186) {
					Displacement(JNLE, DataSize::Byte);
				} else {
					Complete(OUTS, None, None, data_size_);
				}
			break;

			case 0x70: Displacement(JO, DataSize::Byte);	break;
			case 0x71: Displacement(JNO, DataSize::Byte);	break;
			case 0x72: Displacement(JB, DataSize::Byte);	break;
			case 0x73: Displacement(JNB, DataSize::Byte);	break;
			case 0x74: Displacement(JZ, DataSize::Byte);	break;
			case 0x75: Displacement(JNZ, DataSize::Byte);	break;
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

			case 0x90: Complete(NOP, None, None, DataSize::Byte);	break;	// Could be encoded as XCHG AX, AX if Operation space becomes limited.
			case 0x91: Complete(XCHG, eAX, eCX, data_size_);		break;
			case 0x92: Complete(XCHG, eAX, eDX, data_size_);		break;
			case 0x93: Complete(XCHG, eAX, eBX, data_size_);		break;
			case 0x94: Complete(XCHG, eAX, eSP, data_size_);		break;
			case 0x95: Complete(XCHG, eAX, eBP, data_size_);		break;
			case 0x96: Complete(XCHG, eAX, eSI, data_size_);		break;
			case 0x97: Complete(XCHG, eAX, eDI, data_size_);		break;

			case 0x98: Complete(CBW, None, None, data_size_);		break;
			case 0x99: Complete(CWD, None, None, data_size_);		break;
			case 0x9a: Far(CALLfar);								break;
			case 0x9b: Complete(WAIT, None, None, DataSize::Byte);	break;
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

			case 0xc0:
				if constexpr (model >= Model::i80186) {
					ShiftGroup();
					source_ = Source::Immediate;
					operand_size_ = DataSize::Byte;
				} else {
					RegData(RETnear, None, data_size_);
				}
			break;
			case 0xc1:
				if constexpr (model >= Model::i80186) {
					ShiftGroup();
					source_ = Source::Immediate;
					operand_size_ = data_size_;
				} else {
					Complete(RETnear, None, None, DataSize::Byte);
				}
			break;
			case 0xc2: RegData(RETnear, None, data_size_);				break;
			case 0xc3: Complete(RETnear, None, None, DataSize::Byte);	break;
			case 0xc4: MemRegReg(LES, Reg_MemReg, data_size_);			break;
			case 0xc5: MemRegReg(LDS, Reg_MemReg, data_size_);			break;
			case 0xc6: MemRegReg(MOV, MemRegMOV, DataSize::Byte);		break;
			case 0xc7: MemRegReg(MOV, MemRegMOV, data_size_);			break;

			case 0xc8:
				if constexpr (model >= Model::i80186) {
					Displacement16Operand8(ENTER);
				} else {
					RegData(RETfar, None, data_size_);
				}
			break;
			case 0xc9:
				if constexpr (model >= Model::i80186) {
					Complete(LEAVE, None, None, DataSize::Byte);
				} else {
					Complete(RETfar, None, None, DataSize::Word);
				}
			break;

			case 0xca: RegData(RETfar, None, data_size_);				break;
			case 0xcb: Complete(RETfar, None, None, DataSize::Word);	break;

			case 0xcc:
				// Encode INT3 as though it were INT with an
				// immediate operand of 3.
				Complete(INT, Immediate, None, DataSize::Byte);
				operand_ = 3;
			break;
			case 0xcd: RegData(INT, None, DataSize::Byte);			break;
			case 0xce: Complete(INTO, None, None, DataSize::Byte);	break;
			case 0xcf: Complete(IRET, None, None, DataSize::Byte);	break;

			case 0xd0: case 0xd1:
				ShiftGroup();
			break;
			case 0xd2: case 0xd3:
				ShiftGroup();
				source_ = Source::eCX;
			break;
			case 0xd4: RegData(AAM, eAX, DataSize::Byte);			break;
			case 0xd5: RegData(AAD, eAX, DataSize::Byte);			break;
			case 0xd6: Complete(SALC, None, None, DataSize::Byte);	break;
			case 0xd7: Complete(XLAT, None, None, DataSize::Byte);	break;

			case 0xd8: MemRegReg(ESC, Reg_MemReg, data_size_);	break;
			case 0xd9: MemRegReg(ESC, Reg_MemReg, data_size_);	break;
			case 0xda: MemRegReg(ESC, Reg_MemReg, data_size_);	break;
			case 0xdb: MemRegReg(ESC, Reg_MemReg, data_size_);	break;
			case 0xdc: MemRegReg(ESC, Reg_MemReg, data_size_);	break;
			case 0xdd: MemRegReg(ESC, Reg_MemReg, data_size_);	break;
			case 0xde: MemRegReg(ESC, Reg_MemReg, data_size_);	break;
			case 0xdf: MemRegReg(ESC, Reg_MemReg, data_size_);	break;

			case 0xe0: Displacement(LOOPNE, DataSize::Byte);	break;
			case 0xe1: Displacement(LOOPE, DataSize::Byte);		break;
			case 0xe2: Displacement(LOOP, DataSize::Byte);		break;
			case 0xe3: Displacement(JCXZ, DataSize::Byte);		break;

			case 0xe4: RegAddr(IN, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe5: RegAddr(IN, eAX, data_size_, DataSize::Byte);		break;
			case 0xe6: AddrReg(OUT, eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe7: AddrReg(OUT, eAX, data_size_, DataSize::Byte);		break;

			case 0xe8: Displacement(CALLrel, data_size(address_size_));		break;
			case 0xe9: Displacement(JMPrel, data_size(address_size_));		break;
			case 0xea: Far(JMPfar);											break;
			case 0xeb: Displacement(JMPrel, DataSize::Byte);				break;

			case 0xec: Complete(IN, eDX, eAX, DataSize::Byte);	break;
			case 0xed: Complete(IN, eDX, eAX, data_size_);		break;
			case 0xee: Complete(OUT, eAX, eDX, DataSize::Byte);	break;
			case 0xef: Complete(OUT, eAX, eDX, data_size_);		break;

			case 0xf0: lock_ = true;					break;
			// Unused: 0xf1
			case 0xf2: repetition_ = Repetition::RepNE;	break;
			case 0xf3: repetition_ = Repetition::RepE;	break;

			case 0xf4: Complete(HLT, None, None, DataSize::Byte);				break;
			case 0xf5: Complete(CMC, None, None, DataSize::Byte);				break;
			case 0xf6: MemRegReg(Invalid, MemRegTEST_to_IDIV, DataSize::Byte);	break;
			case 0xf7: MemRegReg(Invalid, MemRegTEST_to_IDIV, data_size_);		break;

			case 0xf8: Complete(CLC, None, None, DataSize::Byte);	break;
			case 0xf9: Complete(STC, None, None, DataSize::Byte);	break;
			case 0xfa: Complete(CLI, None, None, DataSize::Byte);	break;
			case 0xfb: Complete(STI, None, None, DataSize::Byte);	break;
			case 0xfc: Complete(CLD, None, None, DataSize::Byte);	break;
			case 0xfd: Complete(STD, None, None, DataSize::Byte);	break;

			case 0xfe: MemRegReg(Invalid, MemRegINC_DEC, DataSize::Byte);	break;
			case 0xff: MemRegReg(Invalid, MemRegINC_to_PUSH, data_size_);	break;
		}
	}

	// MARK: - Additional F page of instructions.

	if constexpr (model >= Model::i80286) {
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
					Complete(LOADALL, None, None, DataSize::Byte);
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
				case 0x74: RequiresMin(i80386);	Displacement(JZ, data_size_);	break;
				case 0x75: RequiresMin(i80386);	Displacement(JNZ, data_size_);	break;
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
				case 0xa1: RequiresMin(i80386);	Complete(POP, None, FS, data_size_);	break;
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
				case 0xa9: RequiresMin(i80386);	Complete(POP, None, GS, data_size_);	break;
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

		// These tables are fairly redundant due to the register ordering within
		// Source, but act to improve readability and permit further Source
		// reordering in the future.
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
		} else if(rm == 6 && mod == 0) {
			// There's no BP direct; BP with ostensibly no offset means 'direct address' mode.
			displacement_size_ = data_size(address_size_);
			memreg = Source::DirectAddress;
		} else {
			const DataSize sizes[] = {
				DataSize::None,
				DataSize::Byte,
				data_size(address_size_)
			};
			displacement_size_ = sizes[mod];

			if(is_32bit(model) && address_size_ == AddressSize::b32) {
				// 32-bit decoding: the range of potential indirections is expanded,
				// and may segue into obtaining a SIB.
				sib_ = ScaleIndexBase(0, Source::None, reg_table[rm]);
				expects_sib = rm == 4;	// Indirect via eSP isn't directly supported; it's the
										// escape indicator for reading a SIB.
				memreg = Source::Indirect;
			} else {
				// Classic 16-bit decoding: mode picks a displacement size,
				// and a few fixed index+base pairs are defined.
				//
				// A base of eAX is meaningless, with the source type being the indicator
				// that it should be ignored. ScaleIndexBase can't store a base of Source::None.
				constexpr ScaleIndexBase rm_table[8] = {
					ScaleIndexBase(0, Source::eSI, Source::eBX),
					ScaleIndexBase(0, Source::eDI, Source::eBX),
					ScaleIndexBase(0, Source::eSI, Source::eBP),
					ScaleIndexBase(0, Source::eDI, Source::eBP),
					ScaleIndexBase(0, Source::eSI, Source::eAX),
					ScaleIndexBase(0, Source::eDI, Source::eAX),
					ScaleIndexBase(0, Source::None, Source::eBP),
					ScaleIndexBase(0, Source::eBX, Source::eAX),
				};

				sib_ = rm_table[rm];
				memreg = (rm >= 4 && rm != 6) ? Source::IndirectNoBase : Source::Indirect;
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
				source_ = memreg;

				switch(reg) {
					default:
						// case 1 is treated as another form of TEST on the 8086.
						// (and, I guess, the 80186?)
						if constexpr (model >= Model::i80286) {
							undefined();
						}
						[[fallthrough]];

					case 0:
						destination_ = memreg;
						source_ = Source::Immediate;
						operand_size_ = operation_size_;
						SetOperation(Operation::TEST);
					break;
					case 2:		SetOperation(Operation::NOT);		break;
					case 3:		SetOperation(Operation::NEG);		break;
					case 4:		SetOperation(Operation::MUL);		break;
					case 5:		SetOperation(Operation::IMUL_1);	break;
					case 6:		SetOperation(Operation::DIV);		break;
					case 7:		SetOperation(Operation::IDIV);		break;
				}
			break;

			case ModRegRMFormat::Seg_MemReg:
			case ModRegRMFormat::MemReg_Seg: {
				// On the 8086, only two bits of reg are used.
				const int masked_reg = model >= Model::i80286 ? reg : reg & 3;

				// The 16-bit chips have four segment registers;
				// the 80386 onwards has six.
				if constexpr (is_32bit(model)) {
					if(masked_reg > 5) {
						undefined();
					}
				} else {
					if(masked_reg > 3) {
						undefined();
					}
				}

				if(modregrm_format_ == ModRegRMFormat::Seg_MemReg) {
					source_ = memreg;
					destination_ = seg_table[masked_reg];

					// 80286 and later disallow MOV to CS.
					if(model >= Model::i80286 && destination_ == Source::CS) {
						undefined();
					}
				} else {
					source_ = seg_table[masked_reg];
					destination_ = memreg;
				}
			} break;

			case ModRegRMFormat::MemRegROL_to_SAR:
				destination_ = memreg;

				switch(reg) {
					default:
						if constexpr (model == Model::i8086) {
							if(source_ == Source::eCX) {
								SetOperation(Operation::SETMOC);
							} else {
								SetOperation(Operation::SETMO);
							}
						} else {
							undefined();
						}
					break;

					case 0:		SetOperation(Operation::ROL);	break;
					case 1:		SetOperation(Operation::ROR);	break;
					case 2:		SetOperation(Operation::RCL);	break;
					case 3:		SetOperation(Operation::RCR);	break;
					case 4:		SetOperation(Operation::SAL);	break;
					case 5:		SetOperation(Operation::SHR);	break;
					case 7:		SetOperation(Operation::SAR);	break;
				}
			break;

			case ModRegRMFormat::MemRegINC_DEC:
				source_ = destination_ = memreg;

				switch(reg) {
					default:	undefined();

					case 0:		SetOperation(Operation::INC);	break;
					case 1:		SetOperation(Operation::DEC);	break;
				}
			break;

			case ModRegRMFormat::MemRegINC_to_PUSH:
				source_ = destination_ = memreg;

				switch(reg) {
					default:
						// case 7 is treated as another form of PUSH on the 8086.
						// (and, I guess, the 80186?)
						if constexpr (model >= Model::i80286) {
							undefined();
						}
						[[fallthrough]];
					case 6:	SetOperation(Operation::PUSH);		break;

					case 0:	SetOperation(Operation::INC);		break;
					case 1:	SetOperation(Operation::DEC);		break;
					case 2:	SetOperation(Operation::CALLabs);	break;
					case 3:	SetOperation(Operation::CALLfar);	break;
					case 4:	SetOperation(Operation::JMPabs);	break;
					case 5:	SetOperation(Operation::JMPfar);	break;
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
				sign_extend_operand_ = true;	// Will be effective only if modregrm_format_ == ModRegRMFormat::MemRegADD_to_CMP_SignExtend.

				switch(reg) {
					default:	SetOperation(Operation::ADD);	break;
					case 1:		SetOperation(Operation::OR);	break;
					case 2:		SetOperation(Operation::ADC);	break;
					case 3:		SetOperation(Operation::SBB);	break;
					case 4:		SetOperation(Operation::AND);	break;
					case 5:		SetOperation(Operation::SUB);	break;
					case 6:		SetOperation(Operation::XOR);	break;
					case 7:		SetOperation(Operation::CMP);	break;
				}
			break;

			case ModRegRMFormat::MemRegSLDT_to_VERW:
				destination_ = source_ = memreg;

				switch(reg) {
					default: undefined();

					case 0:		SetOperation(Operation::SLDT);	break;
					case 1:		SetOperation(Operation::STR);	break;
					case 2:		SetOperation(Operation::LLDT);	break;
					case 3:		SetOperation(Operation::LTR);	break;
					case 4:		SetOperation(Operation::VERR);	break;
					case 5:		SetOperation(Operation::VERW);	break;
				}
			break;

			case ModRegRMFormat::MemRegSGDT_to_LMSW:
				destination_ = source_ = memreg;

				switch(reg) {
					default: undefined();

					case 0:		SetOperation(Operation::SGDT);	break;
					case 1:		SetOperation(Operation::SIDT);	break;
					case 2:		SetOperation(Operation::LGDT);	break;
					case 3:		SetOperation(Operation::LIDT);	break;
					case 4:		SetOperation(Operation::SMSW);	break;
					case 6:		SetOperation(Operation::LMSW);	break;
				}
			break;

			case ModRegRMFormat::MemRegBT_to_BTC:
				destination_ = memreg;
				source_ = Source::Immediate;
				operand_size_ = DataSize::Byte;

				switch(reg) {
					default:	undefined();

					case 4:		SetOperation(Operation::BT);	break;
					case 5:		SetOperation(Operation::BTS);	break;
					case 6:		SetOperation(Operation::BTR);	break;
					case 7:		SetOperation(Operation::BTC);	break;
				}
			break;

			default: assert(false);
		}

		if(expects_sib && (source_ == Source::Indirect || destination_ == Source::Indirect)) {
			phase_ = Phase::ScaleIndexBase;
		} else {
			phase_ = (displacement_size_ != DataSize::None || operand_size_ != DataSize::None) ? Phase::DisplacementOrOperand : Phase::ReadyToPost;
		}
	}

#undef undefined
#undef SetOperation

	// MARK: - ScaleIndexBase

	if constexpr (is_32bit(model)) {
		if(phase_ == Phase::ScaleIndexBase && source != end) {
			sib_ = *source;
			++source;
			++consumed_;

			// Potentially record the lack of a base.
			if(displacement_size_ == DataSize::None && (uint8_t(sib_)&7) == 5) {
				source_ = (source_ == Source::Indirect) ? Source::IndirectNoBase : source_;
				destination_ = (destination_ == Source::Indirect) ? Source::IndirectNoBase : destination_;
			}

			phase_ = (displacement_size_ != DataSize::None || operand_size_ != DataSize::None) ? Phase::DisplacementOrOperand : Phase::ReadyToPost;
		}
	}

	// MARK: - Displacement and operand.

	if(phase_ == Phase::DisplacementOrOperand) {
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

			if(!sign_extend_displacement_) {
				switch(displacement_size_) {
					case DataSize::None:	displacement_ = 0;						break;
					case DataSize::Byte:	displacement_ = uint8_t(inward_data_);	break;
					case DataSize::Word:	displacement_ = uint16_t(inward_data_);	break;
					case DataSize::DWord:	displacement_ = int32_t(inward_data_);	break;
				}
			} else {
				switch(displacement_size_) {
					case DataSize::None:	displacement_ = 0;						break;
					case DataSize::Byte:	displacement_ = int8_t(inward_data_);	break;
					case DataSize::Word:	displacement_ = int16_t(inward_data_);	break;
					case DataSize::DWord:	displacement_ = int32_t(inward_data_);	break;
				}
			}
			inward_data_ >>= bit_size(displacement_size_);

			// Use inequality of sizes as a test for necessary sign extension.
			if(operand_size_ == data_size_ || !sign_extend_operand_) {
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
		// TODO: map to #UD where applicable; build LOCK into the Operation type, buying an extra bit for the operation?
		//
		// As of the P6 Intel stipulates that:
		//
		// "The LOCK prefix can be prepended only to the following instructions and to those forms of the instructions
		// that use a memory operand: ADD, ADC, AND, BTC, BTR, BTS, CMPXCHG, DEC, INC, NEG, NOT, OR, SBB, SUB, XOR,
		// XADD, and XCHG."
		//
		// ... and the #UD exception will be raised if LOCK is encountered elsewhere. So adding 17 additional
		// operations would unlock an extra bit of storage for a net gain of 239 extra operation types and thereby
		// alleviating any concerns over whether there'll be space to handle MMX, floating point extensions, etc.

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
				operation_size_,
				static_cast<typename InstructionT::DisplacementT>(displacement_),
				static_cast<typename InstructionT::ImmediateT>(operand_)
			)
		);
		reset_parsing();
		return result;
	}

	// Check for a too-long instruction.
	if(consumed_ == max_instruction_length) {
		std::pair<int, InstructionT> result;
		if(max_instruction_length == 65536) {
			result = std::make_pair(consumed_, InstructionT(Operation::NOP));
		} else {
			result = std::make_pair(consumed_, InstructionT());
		}
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

std::pair<int, Instruction<false>> Decoder8086::decode(const uint8_t *source, std::size_t length) {
	return decoder.decode(source, length);
}
