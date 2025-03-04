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
std::pair<int, typename Decoder<model>::InstructionT> Decoder<model>::decode(
	const uint8_t *source,
	const std::size_t length
) {
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

#define Requires(x)		if constexpr (model != Model::x) return undefined();
#define RequiresMin(x)	if constexpr (model < Model::x) return undefined();

	while(phase_ == Phase::Instruction && source != end) {
		const uint8_t instr = *source;
		++source;
		++consumed_;

		switch(instr) {
			default: return undefined();

#define PartialBlock(start, operation)																\
	case start + 0x00: mem_reg_reg(operation, ModRegRMFormat::MemReg_Reg, DataSize::Byte);	break;	\
	case start + 0x01: mem_reg_reg(operation, ModRegRMFormat::MemReg_Reg, data_size_);		break;	\
	case start + 0x02: mem_reg_reg(operation, ModRegRMFormat::Reg_MemReg, DataSize::Byte);	break;	\
	case start + 0x03: mem_reg_reg(operation, ModRegRMFormat::Reg_MemReg, data_size_);		break;	\
	case start + 0x04: reg_data(operation, Source::eAX, DataSize::Byte);					break;	\
	case start + 0x05: reg_data(operation, Source::eAX, data_size_);						break;

			PartialBlock(0x00, Operation::ADD);
			case 0x06: complete(Operation::PUSH, Source::ES, Source::None, data_size_);	break;
			case 0x07: complete(Operation::POP, Source::None, Source::ES, data_size_);	break;

			PartialBlock(0x08, Operation::OR);
			case 0x0e: complete(Operation::PUSH, Source::CS, Source::None, data_size_);	break;

			// The 286 onwards have a further set of instructions
			// prefixed with $0f.
			case 0x0f:
				if constexpr (model < Model::i80286) {
					complete(Operation::POP, Source::None, Source::CS, data_size_);
				} else {
					phase_ = Phase::InstructionPageF;
				}
			break;

			PartialBlock(0x10, Operation::ADC);
			case 0x16: complete(Operation::PUSH, Source::SS, Source::None, DataSize::Word);		break;
			case 0x17: complete(Operation::POP, Source::None, Source::SS, DataSize::Word);		break;

			PartialBlock(0x18, Operation::SBB);
			case 0x1e: complete(Operation::PUSH, Source::DS, Source::None, DataSize::Word);		break;
			case 0x1f: complete(Operation::POP, Source::None, Source::DS, DataSize::Word);		break;

			PartialBlock(0x20, Operation::AND);
			case 0x26: segment_override_ = Source::ES;											break;
			case 0x27: complete(Operation::DAA, Source::None, Source::None, DataSize::Byte);	break;

			PartialBlock(0x28, Operation::SUB);
			case 0x2e: segment_override_ = Source::CS;											break;
			case 0x2f: complete(Operation::DAS, Source::None, Source::None, DataSize::Byte);	break;

			PartialBlock(0x30, Operation::XOR);
			case 0x36: segment_override_ = Source::SS;											break;
			case 0x37: complete(Operation::AAA, Source::None, Source::None, DataSize::Word);	break;

			PartialBlock(0x38, Operation::CMP);
			case 0x3e: segment_override_ = Source::DS;											break;
			case 0x3f: complete(Operation::AAS, Source::None, Source::None, DataSize::Word);	break;

#undef PartialBlock

#define RegisterBlock(start, operation)														\
	case start + 0x00: complete(operation, Source::eAX, Source::eAX, data_size_);	break;	\
	case start + 0x01: complete(operation, Source::eCX, Source::eCX, data_size_);	break;	\
	case start + 0x02: complete(operation, Source::eDX, Source::eDX, data_size_);	break;	\
	case start + 0x03: complete(operation, Source::eBX, Source::eBX, data_size_);	break;	\
	case start + 0x04: complete(operation, Source::eSP, Source::eSP, data_size_);	break;	\
	case start + 0x05: complete(operation, Source::eBP, Source::eBP, data_size_);	break;	\
	case start + 0x06: complete(operation, Source::eSI, Source::eSI, data_size_);	break;	\
	case start + 0x07: complete(operation, Source::eDI, Source::eDI, data_size_);	break;

			RegisterBlock(0x40, Operation::INC);
			RegisterBlock(0x48, Operation::DEC);
			RegisterBlock(0x50, Operation::PUSH);
			RegisterBlock(0x58, Operation::POP);

#undef RegisterBlock

			case 0x60:
				if constexpr (model < Model::i80186) {
					displacement(Operation::JO, DataSize::Byte);
				} else {
					complete(Operation::PUSHA, Source::None, Source::None, data_size_);
				}
			break;
			case 0x61:
				if constexpr (model < Model::i80186) {
					displacement(Operation::JNO, DataSize::Byte);
				} else {
					complete(Operation::POPA, Source::None, Source::None, data_size_);
				}
			break;
			case 0x62:
				if constexpr (model < Model::i80186) {
					displacement(Operation::JB, DataSize::Byte);
				} else {
					mem_reg_reg(Operation::BOUND, ModRegRMFormat::Reg_MemReg, data_size_);
				}
			break;
			case 0x63:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JNB, DataSize::Byte);
				} else {
					mem_reg_reg(Operation::ARPL, ModRegRMFormat::MemReg_Reg, DataSize::Word);
				}
			break;
			case 0x64:
				if constexpr (model < Model::i80386) {
					displacement(Operation::JZ, DataSize::Byte);
				} else {
					RequiresMin(i80386);
					segment_override_ = Source::FS;
				}
			break;
			case 0x65:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JNZ, DataSize::Byte);
					break;
				}
				RequiresMin(i80386);
				segment_override_ = Source::GS;
			break;
			case 0x66:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JBE, DataSize::Byte);
					break;
				}
				RequiresMin(i80386);
				data_size_ = DataSize(int(default_data_size_) ^ int(DataSize::Word) ^ int(DataSize::DWord));
			break;
			case 0x67:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JNBE, DataSize::Byte);
					break;
				}
				RequiresMin(i80386);
				address_size_ = AddressSize(int(default_address_size_) ^ int(AddressSize::b16) ^ int(AddressSize::b32));
			break;
			case 0x68:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JS, DataSize::Byte);
				} else {
					immediate(Operation::PUSH, data_size_);
					operation_size_ = data_size_;
				}
			break;
			case 0x69:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JNS, DataSize::Byte);
				} else {
					mem_reg_reg(Operation::IMUL_3, ModRegRMFormat::Reg_MemReg, data_size_);
					operand_size_ = data_size_;
				}
			break;
			case 0x6a:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JP, DataSize::Byte);
				} else {
					immediate(Operation::PUSH, DataSize::Byte);
				}
			break;
			case 0x6b:
				if constexpr (model < Model::i80286) {
					displacement(Operation::JNP, DataSize::Byte);
				} else {
					mem_reg_reg(Operation::IMUL_3, ModRegRMFormat::Reg_MemReg, data_size_);
					operand_size_ = DataSize::Byte;
					sign_extend_operand_ = true;
				}
			break;
			case 0x6c:	// INSB
				if constexpr (model < Model::i80186) {
					displacement(Operation::JL, DataSize::Byte);
				} else {
					complete(Operation::INS, Source::None, Source::None, DataSize::Byte);
				}
			break;
			case 0x6d:	// INSW/INSD
				if constexpr (model < Model::i80186) {
					displacement(Operation::JNL, DataSize::Byte);
				} else {
					complete(Operation::INS, Source::None, Source::None, data_size_);
				}
			break;
			case 0x6e:	// OUTSB
				if constexpr (model < Model::i80186) {
					displacement(Operation::JLE, DataSize::Byte);
				} else {
					complete(Operation::OUTS, Source::None, Source::None, DataSize::Byte);
				}
			break;
			case 0x6f:	// OUTSW/OUSD
				if constexpr (model < Model::i80186) {
					displacement(Operation::JNLE, DataSize::Byte);
				} else {
					complete(Operation::OUTS, Source::None, Source::None, data_size_);
				}
			break;

			case 0x70: displacement(Operation::JO, DataSize::Byte);		break;
			case 0x71: displacement(Operation::JNO, DataSize::Byte);	break;
			case 0x72: displacement(Operation::JB, DataSize::Byte);		break;
			case 0x73: displacement(Operation::JNB, DataSize::Byte);	break;
			case 0x74: displacement(Operation::JZ, DataSize::Byte);		break;
			case 0x75: displacement(Operation::JNZ, DataSize::Byte);	break;
			case 0x76: displacement(Operation::JBE, DataSize::Byte);	break;
			case 0x77: displacement(Operation::JNBE, DataSize::Byte);	break;
			case 0x78: displacement(Operation::JS, DataSize::Byte);		break;
			case 0x79: displacement(Operation::JNS, DataSize::Byte);	break;
			case 0x7a: displacement(Operation::JP, DataSize::Byte);		break;
			case 0x7b: displacement(Operation::JNP, DataSize::Byte);	break;
			case 0x7c: displacement(Operation::JL, DataSize::Byte);		break;
			case 0x7d: displacement(Operation::JNL, DataSize::Byte);	break;
			case 0x7e: displacement(Operation::JLE, DataSize::Byte);	break;
			case 0x7f: displacement(Operation::JNLE, DataSize::Byte);	break;

			case 0x80:
				mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegADD_to_CMP, DataSize::Byte);
			break;
			case 0x81:
				mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegADD_to_CMP, data_size_);
			break;
			case 0x82:
				mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegADD_to_CMP_SignExtend, DataSize::Byte);
			break;
			case 0x83:
				mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegADD_to_CMP_SignExtend, data_size_);
			break;

			case 0x84: mem_reg_reg(Operation::TEST, ModRegRMFormat::MemReg_Reg, DataSize::Byte);		break;
			case 0x85: mem_reg_reg(Operation::TEST, ModRegRMFormat::MemReg_Reg, data_size_);			break;
			case 0x86: mem_reg_reg(Operation::XCHG, ModRegRMFormat::Reg_MemReg, DataSize::Byte);		break;
			case 0x87: mem_reg_reg(Operation::XCHG, ModRegRMFormat::Reg_MemReg, data_size_);			break;
			case 0x88: mem_reg_reg(Operation::MOV, ModRegRMFormat::MemReg_Reg, DataSize::Byte);			break;
			case 0x89: mem_reg_reg(Operation::MOV, ModRegRMFormat::MemReg_Reg, data_size_);				break;
			case 0x8a: mem_reg_reg(Operation::MOV, ModRegRMFormat::Reg_MemReg, DataSize::Byte);			break;
			case 0x8b: mem_reg_reg(Operation::MOV, ModRegRMFormat::Reg_MemReg, data_size_);				break;
			case 0x8c: mem_reg_reg(Operation::MOV, ModRegRMFormat::MemReg_Seg, DataSize::Word);			break;
			case 0x8d: mem_reg_reg(Operation::LEA, ModRegRMFormat::Reg_MemReg, data_size_);				break;
			case 0x8e: mem_reg_reg(Operation::MOV, ModRegRMFormat::Seg_MemReg, DataSize::Word);			break;
			case 0x8f: mem_reg_reg(Operation::POP, ModRegRMFormat::MemRegSingleOperand, data_size_);	break;

			// NOP could be encoded as XCHG AX, AX if Operation space becomes limited.
			case 0x90: complete(Operation::NOP, Source::None, Source::None, DataSize::Byte);	break;
			case 0x91: complete(Operation::XCHG, Source::eAX, Source::eCX, data_size_);			break;
			case 0x92: complete(Operation::XCHG, Source::eAX, Source::eDX, data_size_);			break;
			case 0x93: complete(Operation::XCHG, Source::eAX, Source::eBX, data_size_);			break;
			case 0x94: complete(Operation::XCHG, Source::eAX, Source::eSP, data_size_);			break;
			case 0x95: complete(Operation::XCHG, Source::eAX, Source::eBP, data_size_);			break;
			case 0x96: complete(Operation::XCHG, Source::eAX, Source::eSI, data_size_);			break;
			case 0x97: complete(Operation::XCHG, Source::eAX, Source::eDI, data_size_);			break;

			case 0x98: complete(Operation::CBW, Source::None, Source::None, data_size_);		break;
			case 0x99: complete(Operation::CWD, Source::None, Source::None, data_size_);		break;
			case 0x9a: far(Operation::CALLfar);													break;
			case 0x9b: complete(Operation::WAIT, Source::None, Source::None, DataSize::Byte);	break;
			case 0x9c: complete(Operation::PUSHF, Source::None, Source::None, data_size_);		break;
			case 0x9d: complete(Operation::POPF, Source::None, Source::None, data_size_);		break;
			case 0x9e: complete(Operation::SAHF, Source::None, Source::None, DataSize::Byte);	break;
			case 0x9f: complete(Operation::LAHF, Source::None, Source::None, DataSize::Byte);	break;

			case 0xa0: reg_addr(Operation::MOV, Source::eAX, DataSize::Byte, data_size(address_size_));	break;
			case 0xa1: reg_addr(Operation::MOV, Source::eAX, data_size_, data_size(address_size_));		break;
			case 0xa2: addr_reg(Operation::MOV, Source::eAX, DataSize::Byte, data_size(address_size_));	break;
			case 0xa3: addr_reg(Operation::MOV, Source::eAX, data_size_, data_size(address_size_));		break;

			case 0xa4: complete(Operation::MOVS, Source::None, Source::None, DataSize::Byte);	break;
			case 0xa5: complete(Operation::MOVS, Source::None, Source::None, data_size_);		break;
			case 0xa6: complete(Operation::CMPS, Source::None, Source::None, DataSize::Byte);	break;
			case 0xa7: complete(Operation::CMPS, Source::None, Source::None, data_size_);		break;
			case 0xa8: reg_data(Operation::TEST, Source::eAX, DataSize::Byte);					break;
			case 0xa9: reg_data(Operation::TEST, Source::eAX, data_size_);						break;
			case 0xaa: complete(Operation::STOS, Source::None, Source::None, DataSize::Byte);	break;
			case 0xab: complete(Operation::STOS, Source::None, Source::None, data_size_);		break;
			case 0xac: complete(Operation::LODS, Source::None, Source::None, DataSize::Byte);	break;
			case 0xad: complete(Operation::LODS, Source::None, Source::None, data_size_);		break;
			case 0xae: complete(Operation::SCAS, Source::None, Source::None, DataSize::Byte);	break;
			case 0xaf: complete(Operation::SCAS, Source::None, Source::None, data_size_);		break;

			case 0xb0: reg_data(Operation::MOV, Source::eAX, DataSize::Byte);	break;
			case 0xb1: reg_data(Operation::MOV, Source::eCX, DataSize::Byte);	break;
			case 0xb2: reg_data(Operation::MOV, Source::eDX, DataSize::Byte);	break;
			case 0xb3: reg_data(Operation::MOV, Source::eBX, DataSize::Byte);	break;
			case 0xb4: reg_data(Operation::MOV, Source::AH, DataSize::Byte);	break;
			case 0xb5: reg_data(Operation::MOV, Source::CH, DataSize::Byte);	break;
			case 0xb6: reg_data(Operation::MOV, Source::DH, DataSize::Byte);	break;
			case 0xb7: reg_data(Operation::MOV, Source::BH, DataSize::Byte);	break;
			case 0xb8: reg_data(Operation::MOV, Source::eAX, data_size_);		break;
			case 0xb9: reg_data(Operation::MOV, Source::eCX, data_size_);		break;
			case 0xba: reg_data(Operation::MOV, Source::eDX, data_size_);		break;
			case 0xbb: reg_data(Operation::MOV, Source::eBX, data_size_);		break;
			case 0xbc: reg_data(Operation::MOV, Source::eSP, data_size_);		break;
			case 0xbd: reg_data(Operation::MOV, Source::eBP, data_size_);		break;
			case 0xbe: reg_data(Operation::MOV, Source::eSI, data_size_);		break;
			case 0xbf: reg_data(Operation::MOV, Source::eDI, data_size_);		break;

			case 0xc0:
				if constexpr (model >= Model::i80186) {
					shift_group(instr & 1);
					source_ = Source::Immediate;
					operand_size_ = DataSize::Byte;
				} else {
					reg_data(Operation::RETnear, Source::None, data_size_);
				}
			break;
			case 0xc1:
				if constexpr (model >= Model::i80186) {
					shift_group(instr & 1);
					source_ = Source::Immediate;
					operand_size_ = data_size_;
				} else {
					complete(Operation::RETnear, Source::None, Source::None, DataSize::Byte);
				}
			break;
			case 0xc2: reg_data(Operation::RETnear, Source::None, data_size_);						break;
			case 0xc3: complete(Operation::RETnear, Source::None, Source::None, DataSize::Byte);	break;
			case 0xc4: mem_reg_reg(Operation::LES, ModRegRMFormat::Reg_MemReg, data_size_);			break;
			case 0xc5: mem_reg_reg(Operation::LDS, ModRegRMFormat::Reg_MemReg, data_size_);			break;
			case 0xc6: mem_reg_reg(Operation::MOV, ModRegRMFormat::MemRegMOV, DataSize::Byte);		break;
			case 0xc7: mem_reg_reg(Operation::MOV, ModRegRMFormat::MemRegMOV, data_size_);			break;

			case 0xc8:
				if constexpr (model >= Model::i80186) {
					word_displacement_byte_operand(Operation::ENTER);
				} else {
					reg_data(Operation::RETfar, Source::None, data_size_);
				}
			break;
			case 0xc9:
				if constexpr (model >= Model::i80186) {
					complete(Operation::LEAVE, Source::None, Source::None, DataSize::Byte);
				} else {
					complete(Operation::RETfar, Source::None, Source::None, DataSize::Word);
				}
			break;

			case 0xca: reg_data(Operation::RETfar, Source::None, data_size_);					break;
			case 0xcb: complete(Operation::RETfar, Source::None, Source::None, DataSize::Word);	break;

			case 0xcc:
				// Encode INT3 as though it were INT with an
				// immediate operand of 3.
				complete(Operation::INT, Source::Immediate, Source::None, DataSize::Byte);
				operand_ = 3;
			break;
			case 0xcd: reg_data(Operation::INT, Source::None, DataSize::Byte);					break;
			case 0xce: complete(Operation::INTO, Source::None, Source::None, DataSize::Byte);	break;
			case 0xcf: complete(Operation::IRET, Source::None, Source::None, DataSize::Byte);	break;

			case 0xd0: case 0xd1:
				shift_group(instr & 1);
			break;
			case 0xd2: case 0xd3:
				shift_group(instr & 1);
				source_ = Source::eCX;
			break;
			case 0xd4: reg_data(Operation::AAM, Source::eAX, DataSize::Byte);					break;
			case 0xd5: reg_data(Operation::AAD, Source::eAX, DataSize::Byte);					break;
			case 0xd6: complete(Operation::SALC, Source::None, Source::None, DataSize::Byte);	break;
			case 0xd7: complete(Operation::XLAT, Source::None, Source::None, DataSize::Byte);	break;

			case 0xd8: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;
			case 0xd9: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;
			case 0xda: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;
			case 0xdb: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;
			case 0xdc: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;
			case 0xdd: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;
			case 0xde: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;
			case 0xdf: mem_reg_reg(Operation::ESC, ModRegRMFormat::Reg_MemReg, data_size_);	break;

			case 0xe0: displacement(Operation::LOOPNE, DataSize::Byte);		break;
			case 0xe1: displacement(Operation::LOOPE, DataSize::Byte);		break;
			case 0xe2: displacement(Operation::LOOP, DataSize::Byte);		break;
			case 0xe3: displacement(Operation::JCXZ, DataSize::Byte);		break;

			case 0xe4: reg_addr(Operation::IN, Source::eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe5: reg_addr(Operation::IN, Source::eAX, data_size_, DataSize::Byte);		break;
			case 0xe6: addr_reg(Operation::OUT, Source::eAX, DataSize::Byte, DataSize::Byte);	break;
			case 0xe7: addr_reg(Operation::OUT, Source::eAX, data_size_, DataSize::Byte);		break;

			case 0xe8: displacement(Operation::CALLrel, data_size(address_size_));		break;
			case 0xe9: displacement(Operation::JMPrel, data_size(address_size_));		break;
			case 0xea: far(Operation::JMPfar);											break;
			case 0xeb: displacement(Operation::JMPrel, DataSize::Byte);					break;

			case 0xec: complete(Operation::IN, Source::eDX, Source::eAX, DataSize::Byte);	break;
			case 0xed: complete(Operation::IN, Source::eDX, Source::eAX, data_size_);		break;
			case 0xee: complete(Operation::OUT, Source::eAX, Source::eDX, DataSize::Byte);	break;
			case 0xef: complete(Operation::OUT, Source::eAX, Source::eDX, data_size_);		break;

			case 0xf0: lock_ = true;					break;
			// Unused: 0xf1
			case 0xf2: repetition_ = Repetition::RepNE;	break;
			case 0xf3: repetition_ = Repetition::RepE;	break;

			case 0xf4: complete(Operation::HLT, Source::None, Source::None, DataSize::Byte);				break;
			case 0xf5: complete(Operation::CMC, Source::None, Source::None, DataSize::Byte);				break;
			case 0xf6: mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegTEST_to_IDIV, DataSize::Byte);	break;
			case 0xf7: mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegTEST_to_IDIV, data_size_);		break;

			case 0xf8: complete(Operation::CLC, Source::None, Source::None, DataSize::Byte);	break;
			case 0xf9: complete(Operation::STC, Source::None, Source::None, DataSize::Byte);	break;
			case 0xfa: complete(Operation::CLI, Source::None, Source::None, DataSize::Byte);	break;
			case 0xfb: complete(Operation::STI, Source::None, Source::None, DataSize::Byte);	break;
			case 0xfc: complete(Operation::CLD, Source::None, Source::None, DataSize::Byte);	break;
			case 0xfd: complete(Operation::STD, Source::None, Source::None, DataSize::Byte);	break;

			case 0xfe: mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegINC_DEC, DataSize::Byte);	break;
			case 0xff: mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegINC_to_PUSH, data_size_);	break;
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
				default: return undefined();

				case 0x00:	mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegSLDT_to_VERW, data_size_);	break;
				case 0x01:	mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegSGDT_to_LMSW, data_size_);	break;
				case 0x02:	mem_reg_reg(Operation::LAR, ModRegRMFormat::Reg_MemReg, data_size_);				break;
				case 0x03:	mem_reg_reg(Operation::LSL, ModRegRMFormat::Reg_MemReg, data_size_);				break;
				case 0x05:
					Requires(i80286);
					complete(Operation::LOADALL, Source::None, Source::None, DataSize::Byte);
				break;
				case 0x06:	complete(Operation::CLTS, Source::None, Source::None, DataSize::Byte);			break;

				case 0x20:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVfromCr, ModRegRMFormat::Reg_MemReg, DataSize::DWord);
				break;
				case 0x21:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVfromDr, ModRegRMFormat::Reg_MemReg, DataSize::DWord);
				break;
				case 0x22:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVtoCr, ModRegRMFormat::Reg_MemReg, DataSize::DWord);
				break;
				case 0x23:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVtoDr, ModRegRMFormat::Reg_MemReg, DataSize::DWord);
				break;
				case 0x24:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVfromTr, ModRegRMFormat::Reg_MemReg, DataSize::DWord);
				break;
				case 0x26:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVtoTr, ModRegRMFormat::Reg_MemReg, DataSize::DWord);
				break;

				case 0x70: RequiresMin(i80386);	displacement(Operation::JO, data_size_);	break;
				case 0x71: RequiresMin(i80386);	displacement(Operation::JNO, data_size_);	break;
				case 0x72: RequiresMin(i80386);	displacement(Operation::JB, data_size_);	break;
				case 0x73: RequiresMin(i80386);	displacement(Operation::JNB, data_size_);	break;
				case 0x74: RequiresMin(i80386);	displacement(Operation::JZ, data_size_);	break;
				case 0x75: RequiresMin(i80386);	displacement(Operation::JNZ, data_size_);	break;
				case 0x76: RequiresMin(i80386);	displacement(Operation::JBE, data_size_);	break;
				case 0x77: RequiresMin(i80386);	displacement(Operation::JNBE, data_size_);	break;
				case 0x78: RequiresMin(i80386);	displacement(Operation::JS, data_size_);	break;
				case 0x79: RequiresMin(i80386);	displacement(Operation::JNS, data_size_);	break;
				case 0x7a: RequiresMin(i80386);	displacement(Operation::JP, data_size_);	break;
				case 0x7b: RequiresMin(i80386);	displacement(Operation::JNP, data_size_);	break;
				case 0x7c: RequiresMin(i80386);	displacement(Operation::JL, data_size_);	break;
				case 0x7d: RequiresMin(i80386);	displacement(Operation::JNL, data_size_);	break;
				case 0x7e: RequiresMin(i80386);	displacement(Operation::JLE, data_size_);	break;
				case 0x7f: RequiresMin(i80386);	displacement(Operation::JNLE, data_size_);	break;

#define Set(x)												\
	RequiresMin(i80386);									\
	mem_reg_reg(x, ModRegRMFormat::MemRegSingleOperand, DataSize::Byte);

				case 0x90: Set(Operation::SETO);	break;
				case 0x91: Set(Operation::SETNO);	break;
				case 0x92: Set(Operation::SETB);	break;
				case 0x93: Set(Operation::SETNB);	break;
				case 0x94: Set(Operation::SETZ);	break;
				case 0x95: Set(Operation::SETNZ);	break;
				case 0x96: Set(Operation::SETBE);	break;
				case 0x97: Set(Operation::SETNBE);	break;
				case 0x98: Set(Operation::SETS);	break;
				case 0x99: Set(Operation::SETNS);	break;
				case 0x9a: Set(Operation::SETP);	break;
				case 0x9b: Set(Operation::SETNP);	break;
				case 0x9c: Set(Operation::SETL);	break;
				case 0x9d: Set(Operation::SETNL);	break;
				case 0x9e: Set(Operation::SETLE);	break;
				case 0x9f: Set(Operation::SETNLE);	break;

#undef Set

				case 0xa0:
					RequiresMin(i80386);
					complete(Operation::PUSH, Source::FS, Source::None, data_size_);
				break;
				case 0xa1:
					RequiresMin(i80386);
					complete(Operation::POP, Source::None, Source::FS, data_size_);
				break;
				case 0xa3:
					RequiresMin(i80386);
					mem_reg_reg(Operation::BT, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xa4:
					RequiresMin(i80386);
					mem_reg_reg(Operation::SHLDimm, ModRegRMFormat::Reg_MemReg, data_size_);
					operand_size_ = DataSize::Byte;
				break;
				case 0xa5:
					RequiresMin(i80386);
					mem_reg_reg(Operation::SHLDCL, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xa8:
					RequiresMin(i80386);
					complete(Operation::PUSH, Source::GS, Source::None, data_size_);
				break;
				case 0xa9:
					RequiresMin(i80386);
					complete(Operation::POP, Source::None, Source::GS, data_size_);
				break;
				case 0xab:
					RequiresMin(i80386);
					mem_reg_reg(Operation::BTS, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xac:
					RequiresMin(i80386);
					mem_reg_reg(Operation::SHRDimm, ModRegRMFormat::Reg_MemReg, data_size_);
					operand_size_ = DataSize::Byte;
				break;
				case 0xad:
					RequiresMin(i80386);
					mem_reg_reg(Operation::SHRDCL, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xaf:
					RequiresMin(i80386);
					mem_reg_reg(Operation::IMUL_2, ModRegRMFormat::Reg_MemReg, data_size_);
				break;

				case 0xb2:
					RequiresMin(i80386);
					mem_reg_reg(Operation::LSS, ModRegRMFormat::Reg_MemReg, data_size_);
				break;
				case 0xb3:
					RequiresMin(i80386);
					mem_reg_reg(Operation::BTR, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xb4:
					RequiresMin(i80386);
					mem_reg_reg(Operation::LFS, ModRegRMFormat::Reg_MemReg, data_size_);
				break;
				case 0xb5:
					RequiresMin(i80386);
					mem_reg_reg(Operation::LGS, ModRegRMFormat::Reg_MemReg, data_size_);
				break;
				case 0xb6:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVZX, ModRegRMFormat::Reg_MemReg, DataSize::Byte);
				break;
				case 0xb7:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVZX, ModRegRMFormat::Reg_MemReg, DataSize::Word);
				break;
				case 0xba:
					RequiresMin(i80386);
					mem_reg_reg(Operation::Invalid, ModRegRMFormat::MemRegBT_to_BTC, data_size_);
				break;
				case 0xbb:
					RequiresMin(i80386);
					mem_reg_reg(Operation::BTC, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xbc:
					RequiresMin(i80386);
					mem_reg_reg(Operation::BSF, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xbd:
					RequiresMin(i80386);
					mem_reg_reg(Operation::BSR, ModRegRMFormat::MemReg_Reg, data_size_);
				break;
				case 0xbe:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVSX, ModRegRMFormat::Reg_MemReg, DataSize::Byte);
				break;
				case 0xbf:
					RequiresMin(i80386);
					mem_reg_reg(Operation::MOVSX, ModRegRMFormat::Reg_MemReg, DataSize::Word);
				break;
			}
		}
	}

#undef Requires
#undef RequiresMin

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
		static constexpr Source reg_table[8] = {
			Source::eAX,		Source::eCX,		Source::eDX,		Source::eBX,
			Source::eSPorAH,	Source::eBPorCH,	Source::eSIorDH,	Source::eDIorBH,
		};
		static constexpr Source seg_table[6] = {
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
				operation_ == Operation::LFS
			) {
				return undefined();
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
				static constexpr ScaleIndexBase rm_table[8] = {
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
							return undefined();
						}
						[[fallthrough]];

					case 0:
						destination_ = memreg;
						source_ = Source::Immediate;
						operand_size_ = operation_size_;
						set(Operation::TEST);
					break;
					case 2:		set(Operation::NOT);		break;
					case 3:		set(Operation::NEG);		break;
					case 4:		set(Operation::MUL);		break;
					case 5:		set(Operation::IMUL_1);		break;
					case 6:		set(Operation::DIV);		break;
					case 7:		set(Operation::IDIV);		break;
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
						return undefined();
					}
				} else {
					if(masked_reg > 3) {
						return undefined();
					}
				}

				if(modregrm_format_ == ModRegRMFormat::Seg_MemReg) {
					source_ = memreg;
					destination_ = seg_table[masked_reg];

					// 80286 and later disallow MOV to CS.
					if(model >= Model::i80286 && destination_ == Source::CS) {
						return undefined();
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
								set(Operation::SETMOC);
							} else {
								set(Operation::SETMO);
							}
						} else {
							return undefined();
						}
					break;

					case 0:		set(Operation::ROL);	break;
					case 1:		set(Operation::ROR);	break;
					case 2:		set(Operation::RCL);	break;
					case 3:		set(Operation::RCR);	break;
					case 4:		set(Operation::SAL);	break;
					case 5:		set(Operation::SHR);	break;
					case 7:		set(Operation::SAR);	break;
				}
			break;

			case ModRegRMFormat::MemRegINC_DEC:
				source_ = destination_ = memreg;

				switch(reg) {
					default:	return undefined();

					case 0:		set(Operation::INC);	break;
					case 1:		set(Operation::DEC);	break;
				}
			break;

			case ModRegRMFormat::MemRegINC_to_PUSH:
				source_ = destination_ = memreg;

				switch(reg) {
					default:
						// case 7 is treated as another form of PUSH on the 8086.
						// (and, I guess, the 80186?)
						if constexpr (model >= Model::i80286) {
							return undefined();
						}
						[[fallthrough]];
					case 6:	set(Operation::PUSH);		break;

					case 0:	set(Operation::INC);		break;
					case 1:	set(Operation::DEC);		break;
					case 2:	set(Operation::CALLabs);	break;
					case 3:	set(Operation::CALLfar);	break;
					case 4:	set(Operation::JMPabs);		break;
					case 5:	set(Operation::JMPfar);		break;
				}
			break;

			case ModRegRMFormat::MemRegSingleOperand:
				source_ = destination_ = memreg;

				if(reg != 0) {
					return undefined();
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
				operand_size_ =
					(modregrm_format_ == ModRegRMFormat::MemRegADD_to_CMP_SignExtend)
						? DataSize::Byte : operation_size_;

				// sign_extend_operand_ will be effective only if
				// modregrm_format_ == ModRegRMFormat::MemRegADD_to_CMP_SignExtend.
				sign_extend_operand_ = true;

				switch(reg) {
					default:	set(Operation::ADD);	break;
					case 1:		set(Operation::OR);		break;
					case 2:		set(Operation::ADC);	break;
					case 3:		set(Operation::SBB);	break;
					case 4:		set(Operation::AND);	break;
					case 5:		set(Operation::SUB);	break;
					case 6:		set(Operation::XOR);	break;
					case 7:		set(Operation::CMP);	break;
				}
			break;

			case ModRegRMFormat::MemRegSLDT_to_VERW:
				destination_ = source_ = memreg;

				switch(reg) {
					default: 	return undefined();

					case 0:		set(Operation::SLDT);	break;
					case 1:		set(Operation::STR);	break;
					case 2:		set(Operation::LLDT);	break;
					case 3:		set(Operation::LTR);	break;
					case 4:		set(Operation::VERR);	break;
					case 5:		set(Operation::VERW);	break;
				}
			break;

			case ModRegRMFormat::MemRegSGDT_to_LMSW:
				destination_ = source_ = memreg;

				switch(reg) {
					default: 	return undefined();

					case 0:		set(Operation::SGDT);	break;
					case 1:		set(Operation::SIDT);	break;
					case 2:		set(Operation::LGDT);	break;
					case 3:		set(Operation::LIDT);	break;
					case 4:		set(Operation::SMSW);	break;
					case 6:		set(Operation::LMSW);	break;
				}
			break;

			case ModRegRMFormat::MemRegBT_to_BTC:
				destination_ = memreg;
				source_ = Source::Immediate;
				operand_size_ = DataSize::Byte;

				switch(reg) {
					default:	return undefined();

					case 4:		set(Operation::BT);		break;
					case 5:		set(Operation::BTS);	break;
					case 6:		set(Operation::BTR);	break;
					case 7:		set(Operation::BTC);	break;
				}
			break;

			default: assert(false);
		}

		if(expects_sib && (source_ == Source::Indirect || destination_ == Source::Indirect)) {
			phase_ = Phase::ScaleIndexBase;
		} else {
			phase_ =
				(displacement_size_ != DataSize::None || operand_size_ != DataSize::None)
					? Phase::DisplacementOrOperand : Phase::ReadyToPost;
		}
	}

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

			phase_ =
				(displacement_size_ != DataSize::None || operand_size_ != DataSize::None)
					? Phase::DisplacementOrOperand : Phase::ReadyToPost;
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

// Workaround for GCC; despite explicit instantiations above, GCC will fail to
// link without the function below.
void InstructionSet::x86::_gcc_instantiation_workaround() {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder86;
	decoder86.decode(nullptr, 0);

	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i80186> decoder186;
	decoder186.decode(nullptr, 0);

	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i80286> decoder286;
	decoder286.decode(nullptr, 0);

	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i80386> decoder386;
	decoder386.decode(nullptr, 0);
}
