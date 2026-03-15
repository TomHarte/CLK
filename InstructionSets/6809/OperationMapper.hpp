//
//  OperationMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include <utility>

// Cf. https://techheap.packetizer.com/processors/6809/6809Instructions.html
//
// Subject to corrections:
//
//	* CWAI and the pushes and pulls at 0x3x are immediate, not inherent.
namespace InstructionSet::M6809 {

enum class AddressingMode {
	Illegal,

	Inherent,		// No operand.
	Immediate8,		// 8-bit operand.
	Immediate16,	// 16-bit operand.
	Relative8,		// For branches; an 8-bit operand gives a signed offset from the PC.
	Relative16,		// For branches; a 16-bit operand gives an offset from the PC.
	Variant,		// Specialised 'addressing mode' that indicates an instruction map page change.
	Direct,			// An 8-bit operand provides the low byte of an in-memory address. The DPR provides the high.
	Extended,		// Provides a full 16-bit address as an operand.

	// Indexed: a byte follows the instruction, specifying:
	//	b7:	1 = indexed; 0 = 5-bit offset;
	//	b6-b5: register select (00 = X, 01 = Y, 10 = U, 11 = S)
	//	b4: 1 = indirect; 0 = not;
	//	b0–3: sub-mode or 5-bit offset.
	//
	// One or two additional further bytes may then follow.
	Indexed,

	// Instructions with atypical bus activity; things that interact with the stacks.
	Specialised,

	Max,
};

enum class AccessType {
	Read8,
	Read16,
	Write8,
	Write16,
	Modify8,
	LEA,		// Covers both LEA[X/Y/S/U] and JMP, which is LEA to the PC.
	JSR,

	Max,
};

enum class SimpleAddressingMode {
	Illegal,

	Inherent,
	Immediate,
	Relative,
	Variant,
	Direct,
	Extended,
	Indexed,

	Max,
};
constexpr SimpleAddressingMode simplify(const AddressingMode mode) {
	switch(mode) {
		using enum AddressingMode;

		case Illegal:		return SimpleAddressingMode::Illegal;
		case Specialised:
		case Inherent:		return SimpleAddressingMode::Inherent;
		case Immediate8:
		case Immediate16:	return SimpleAddressingMode::Immediate;
		case Relative8:
		case Relative16:	return SimpleAddressingMode::Relative;
		case Variant:		return SimpleAddressingMode::Variant;
		case Direct:		return SimpleAddressingMode::Direct;
		case Extended:		return SimpleAddressingMode::Extended;
		case Indexed:		return SimpleAddressingMode::Indexed;

		default: __builtin_unreachable();
	}
}

enum class Condition {
	A,	N,	HI,	LS,	CC,	CS,	NE,	EQ,
	VC,	VS,	PL,	MI,	GE,	LT,	GT,	LE,
};

enum class Operation {
	None,

	SUBB,	CMPB,	SBCB,	ADDD,	ANDB,	BITB,	LDB,	STB,
	EORB,	ADCB,	ORB,	ADDB,	LDD,	STD,	LDU,	STU,
	SUBA,	CMPA,	SBCA,	SUBD,	ANDA,	BITA,	LDA,	STA,
	EORA,	ADCA,	ORA,	ADDA,	CMPX,	JSR,	LDX,	STX,
	BSR,

	NEG,	COM,	LSR,	ROR,	ASR,
	LSL,	ROL,	DEC,	INC,	TST,	JMP,	CLR,
	NEGA,	COMA,	LSRA,	RORA,	ASRA,
	LSLA,	ROLA,	DECA,	INCA,	TSTA,	CLRA,
	NEGB,	COMB,	LSRB,	RORB,	ASRB,
	LSLB,	ROLB,	DECB,	INCB,	TSTB,	CLRB,

	LEAX,	LEAY,	LEAS,	LEAU,
	PSHS,	PULS,	PSHU,	PULU,
	RTS,	ABX,	RTI,
	CWAI,	MUL,	RESET,	SWI,

	BRA,	BRN,	BHI,	BLS,	BCC,	BCS,	BNE,	BEQ,
	BVC,	BVS,	BPL,	BMI,	BGE,	BLT,	BGT,	BLE,

	Page1,	Page2,	NOP,	SYNC,	LBRA,	LBSR,
	DAA,	ORCC,	ANDCC,	SEX,	EXG,	TFR,

	LBRN,	LBHI,	LBLS,	LBCC,	LBCS,	LBNE,	LBEQ,
	LBVC,	LBVS,	LBPL,	LBMI,	LBGE,	LBLT,	LBGT,	LBLE,

	SWI2,	CMPD,	CMPY,	LDY,	STY,	LDS,	STS,

	SWI3,	CMPU,	CMPS,
};

template <Operation operation>
constexpr bool is_16bit() {
	switch(operation) {
		using enum Operation;

		case SUBB:	case CMPB:	case SBCB:
		case ANDB:	case BITB:	case LDB:	case STB:
		case EORB:	case ADCB:	case ORB:	case ADDB:
		case SUBA:	case CMPA:	case SBCA:
		case ANDA:	case BITA:	case LDA:	case STA:
		case EORA:	case ADCA:	case ORA:	case ADDA:
		case BSR:
		case PSHS:	case PULS:	case PSHU:	case PULU:
		case RTS:	case ABX:	case RTI:
		case CWAI:	case RESET:	case SWI:
		case BRA:	case BRN:	case BHI:	case BLS:
		case BCC:	case BCS:	case BNE:	case BEQ:
		case BVC:	case BVS:	case BPL:	case BMI:
		case BGE:	case BLT:	case BGT:	case BLE:
		case Page1:	case Page2:	case NOP:	case SYNC:
		case DAA:	case ORCC:	case ANDCC:	case SEX:
		case EXG:	case TFR:
		case SWI2:	case SWI3:
			return false;

		case ADDD:	case SUBD:
		case LDD:	case STD:	case LDU:	case STU:
		case CMPX:	case JSR:	case LDX:	case STX:
		case LEAX:	case LEAY:	case LEAS:	case LEAU:
		case MUL:
		case LBRN:	case LBHI:	case LBLS:	case LBCC:
		case LBCS:	case LBNE:	case LBEQ:
		case LBVC:	case LBVS:	case LBPL:	case LBMI:
		case LBGE:	case LBLT:	case LBGT:	case LBLE:
		case LBRA:	case LBSR:
		case CMPD:	case CMPY:	case LDY:	case STY:
		case LDS:	case STS:
		case CMPU:	case CMPS:
		case JMP:
			return true;

		default: return false;
	}
}

enum class AccessGenus {
	Read, Write, Modify, None,
};
template <Operation operation>
constexpr AccessGenus access_genus() {
	switch(operation) {
		using enum Operation;

		default:	// LEA, JSR and JMP — all calculate an effective address but don't (within themselves) access it.
			return AccessGenus::None;

		// Actual reads.
		case SUBB:	case CMPB:	case SBCB:	case ADDD:
		case ANDB:	case BITB:	case EORB:	case ADCB:
		case ORB:	case ADDB:	case LDD:	case LDU:
		case LDB:	case SUBA:	case CMPA:	case SBCA:
		case SUBD:	case ANDA:	case BITA:	case LDA:
		case EORA:	case ADCA:	case ORA:	case ADDA:
		case CMPX:	case LDX:	case BSR:
		case TST:
		case CMPD:	case CMPS:	case CMPU:
			return AccessGenus::Read;

		case STB:	case STD:	case STU:	case STA:
		case STX:	case CLR:	case CLRB:	case STY:
		case STS:
			return AccessGenus::Write;

		case NEG:	case COM:	case LSR:	case ROR:
		case ASR:	case LSL:	case ROL:	case DEC:
		case INC:	case NEGA:	case COMA:	case LSRA:
		case RORA:	case ASRA:	case LSLA:	case ROLA:
		case DECA:	case INCA:	case CLRA:	case NEGB:
		case COMB:	case LSRB:	case RORB:	case ASRB:
		case LSLB:	case ROLB:	case DECB:	case INCB:
			return AccessGenus::Modify;
	}
}

enum class Page {
	Page0, Page1, Page2,
};

/*!
	Utility of geneal usefulness: whatever the @c OperationMapper produces, just return it on the stack.
*/
struct OperationReturner {
	struct MetaOperation {
		Operation operation;
		AddressingMode mode;
		AccessType type;
	};

	template <Operation operation, AddressingMode mode, AccessType type> auto schedule() {
		return MetaOperation(operation, mode, type);
	}
};

namespace {

template <Operation operation, AddressingMode mode, typename SchedulerT>
auto complete(SchedulerT &s) {
	// To do:
	//
	//	(1) upgrade Immediate8 to Immediate16 where justified;
	//	(2) commute [Direct/Extended/Indexed]Read to [Read/Write/Modify] as appropriate.

	static constexpr auto is_16 = is_16bit<operation>();
	static constexpr auto genus = access_genus<operation>();
	static constexpr auto type = [&] {
		switch(genus) {
			default: return operation == Operation::JSR ? AccessType::JSR : AccessType::LEA;
			case AccessGenus::Read: return is_16 ? AccessType::Read16 : AccessType::Read8;
			case AccessGenus::Write: return is_16 ? AccessType::Write16 : AccessType::Write8;
			case AccessGenus::Modify: return AccessType::Modify8;
		}
	} ();

	const auto mapped_mode = [&] {
		switch(mode) {
			default: return mode;
			case AddressingMode::Immediate8: return is_16 ? AddressingMode::Immediate16 : AddressingMode::Immediate8;
		}
	} ();

	return s.template schedule<operation, mapped_mode, type>();
}

}

/*!
	Calls @c scheduler.schedule<Operation,AddressingMode> to describe the instruction
	defined by opcode @c i on page @c page.
*/
template <Page page> struct OperationMapper {
	template <int i, typename SchedulerT> auto dispatch(SchedulerT &scheduler);
};

template <>
template <int i, typename SchedulerT>
auto OperationMapper<Page::Page0>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	static constexpr auto upper = (i >> 4) & 0xf;
	static constexpr auto lower = (i >> 0) & 0xf;

	if constexpr (
		i == 0x87 ||	i == 0x8f ||	i == 0xc7 ||
		i == 0xcd ||	i == 0xcf
	) {
		// Avoid nonsensical:
		//
		//	0x87:	STA immediate
		//	0x8f:	STX immediate
		//	0xc7:	STB immediate
		//	0xcd:	STD immediate
		//	0xcf:	STU immediate.
		return complete<O::None, AM::Illegal>(s);
	}

	static constexpr AddressingMode modes[] = {
		AM::Immediate8, AM::Direct, AM::Indexed, AM::Extended,
	};
	static constexpr auto mode = modes[(i >> 4) & 3];

	switch(upper) {
		default: break;

		case 0x1: {
			static constexpr Operation operations[] = {
				O::Page1,	O::Page2,	O::NOP,		O::SYNC,	O::None,	O::None,	O::LBRA,	O::LBSR,
				O::None,	O::DAA,		O::ORCC,	O::None,	O::ANDCC,	O::SEX,		O::EXG,		O::TFR,
			};
			static constexpr AddressingMode specific_modes[] = {
				AM::Variant,	AM::Variant,	AM::Inherent,	AM::Inherent,
				AM::Illegal,	AM::Illegal,	AM::Relative16,	AM::Relative16,
				AM::Illegal,	AM::Inherent,	AM::Immediate8,	AM::Illegal,
				AM::Immediate8,	AM::Inherent,	AM::Immediate8,	AM::Immediate8,
			};
			return complete<operations[lower], specific_modes[lower]>(s);
		}
		case 0x2: {
			static constexpr Operation operations[] = {
				O::BRA,		O::BRN,		O::BHI,		O::BLS,		O::BCC,		O::BCS,		O::BNE,		O::BEQ,
				O::BVC,		O::BVS,		O::BPL,		O::BMI,		O::BGE,		O::BLT,		O::BGT,		O::BLE,
			};
			return complete<operations[lower], AM::Relative8>(s);
		}
		case 0x3: {
			static constexpr Operation operations[] = {
				O::LEAX,	O::LEAY,	O::LEAS,	O::LEAU,	O::PSHS,	O::PULS,	O::PSHU,	O::PULU,
				O::None,	O::RTS,		O::ABX,		O::RTI,		O::CWAI,	O::MUL,		O::RESET,	O::SWI,
			};
			static constexpr auto op = operations[lower];
			switch(lower) {
				case 0x0:	case 0x1:	case 0x2:	case 0x3:
				return complete<op, AM::Indexed>(s);

				case 0x0a:	case 0x0d:
				return complete<op, AM::Inherent>(s);

				case 0x8:
				return complete<op, AM::Illegal>(s);

				default:
				return complete<op, AM::Specialised>(s);
			}
		}
		case 0x4: {
			static constexpr Operation operations[] = {
				O::NEGA,	O::None,	O::None,	O::COMA,	O::LSRA,	O::None,	O::RORA,	O::ASRA,
				O::LSLA,	O::ROLA,	O::DECA,	O::None,	O::INCA,	O::TSTA,	O::None,	O::CLRA,
			};
			static constexpr auto op = operations[lower];
			return complete<op, op == O::None ? AM::Illegal : AM::Inherent>(s);
		}
		case 0x5: {
			static constexpr Operation operations[] = {
				O::NEGB,	O::None,	O::None,	O::COMB,	O::LSRB,	O::None,	O::RORB,	O::ASRB,
				O::LSLB,	O::ROLB,	O::DECB,	O::None,	O::INCB,	O::TSTB,	O::None,	O::CLRB,
			};
			static constexpr auto op = operations[lower];
			return complete<op, op == O::None ? AM::Illegal : AM::Inherent>(s);
		}
		case 0x0: case 0x6:	case 0x7: {
			static constexpr Operation operations[] = {
				O::NEG,		O::None,	O::None,	O::COM,		O::LSR,		O::None,	O::ROR,		O::ASR,
				O::LSL,		O::ROL,		O::DEC,		O::None,	O::INC,		O::TST,		O::JMP,		O::CLR,
			};
			static constexpr auto op = operations[lower];
			return complete<op, op == O::None ? AM::Illegal : upper == 0 ? AM::Direct : mode>(s);
		}
		case 0x8:	case 0x9:	case 0xa:	case 0xb: {
			static constexpr Operation operations[] = {
				O::SUBA,	O::CMPA,	O::SBCA,	O::SUBD,	O::ANDA,	O::BITA,	O::LDA,		O::STA,
				O::EORA,	O::ADCA,	O::ORA,		O::ADDA,	O::CMPX,	O::JSR,		O::LDX,		O::STX,
			};
			if(i == 0x8d)	return complete<O::BSR, AM::Relative8>(s);
			else			return complete<operations[lower], mode>(s);
		}
		case 0xc:	case 0xd:	case 0xe:	case 0xf: {
			static constexpr Operation operations[] = {
				O::SUBB,	O::CMPB,	O::SBCB,	O::ADDD,	O::ANDB,	O::BITB,	O::LDB,		O::STB,
				O::EORB,	O::ADCB,	O::ORB,		O::ADDB,	O::LDD,		O::STD,		O::LDU,		O::STU,
			};
			return complete<operations[lower], mode>(s);
		}
	}
	__builtin_unreachable();
}

template <>
template <int i, typename SchedulerT>
auto OperationMapper<Page::Page1>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	static constexpr AddressingMode modes[] = {
		AM::Immediate8, AM::Direct, AM::Indexed, AM::Extended,
	};
	static constexpr auto mode = modes[(i >> 4) & 3];

	if constexpr (i >= 0x21 && i < 0x30) {
		static constexpr Operation operations[] = {
						O::LBRN,	O::LBHI,	O::LBLS,	O::LBCC,	O::LBCS,	O::LBNE,	O::LBEQ,
			O::LBVC,	O::LBVS,	O::LBPL,	O::LBMI,	O::LBGE,	O::LBLT,	O::LBGT,	O::LBLE,
		};
		return complete<operations[i - 0x21], AM::Relative16>(s);
	} else switch(i) {
		default:	return complete<O::None, AM::Illegal>(s);
		case 0x3f:	return complete<O::SWI2, AM::Inherent>(s);

		case 0x83:	case 0x93:	case 0xa3:	case 0xb3:
		return complete<O::CMPD, mode>(s);

		case 0x8c:	case 0x9c:	case 0xac:	case 0xbc:
		return complete<O::CMPY, mode>(s);

		case 0x8e:	case 0x9e:	case 0xae:	case 0xbe:
		return complete<O::LDY, mode>(s);

		case 0x9f:	case 0xaf:	case 0xbf:
		return complete<O::STY, mode>(s);

		case 0xce:	case 0xde:	case 0xee:	case 0xfe:
		return complete<O::LDS, mode>(s);

		case 0xdf:	case 0xef:	case 0xff:
		return complete<O::STS, mode>(s);
	}
	__builtin_unreachable();
}

template <>
template <int i, typename SchedulerT>
auto OperationMapper<Page::Page2>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	static constexpr AddressingMode modes[] = {
		AM::Immediate8, AM::Direct, AM::Indexed, AM::Extended,
	};
	static constexpr auto mode = modes[(i >> 4) & 3];

	switch(i) {
		default:	return complete<O::None, AM::Illegal>(s);
		case 0x3f:	return complete<O::SWI3, AM::Inherent>(s);

		case 0x83:	case 0x93:	case 0xa3:	case 0xb3:
		return complete<O::CMPU, mode>(s);

		case 0x8c:	case 0x9c:	case 0xac:	case 0xbc:
		return complete<O::CMPS, mode>(s);
	}
	__builtin_unreachable();
}

}
