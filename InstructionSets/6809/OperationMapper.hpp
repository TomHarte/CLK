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

	Inherent,
	Immediate8,
	Immediate16,
	Direct,
	Relative8,
	Relative16,
	Variant,
	Indexed,
	Extended,

	Max,
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
		case SUBA:	case CMPA:	case SBCA:	case SUBD:
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

		case ADDD:
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
			return true;

		default: return false;
	}
}

enum class Page {
	Page0, Page1, Page2,
};

struct OperationReturner {
	template <Operation operation, AddressingMode mode> auto schedule() {
		return std::make_pair(operation, mode);
	}
};

/*!
	Calls @c scheduler.schedule<Operation,AddressingMode> to describe the instruction
	defined by opcode @c i on page @c page.
*/
template <Page page> struct OperationMapper {
	template <int i, typename SchedulerT> auto dispatch(SchedulerT &scheduler);
};

template <Operation operation, uint8_t opcode>
constexpr AddressingMode mode() {
	using AM = AddressingMode;
	constexpr AddressingMode modes[] = {
		AM::Immediate8, AM::Direct, AM::Indexed, AM::Extended
	};
	constexpr auto prima_facie_mode = modes[(opcode >> 4) & 3];
	constexpr bool is_16 = is_16bit<operation>();

	if constexpr (prima_facie_mode != AM::Immediate8 || !is_16) {
		return prima_facie_mode;
	} else {
		return AM::Immediate16;
	}
};

template <>
template <int i, typename SchedulerT>
auto OperationMapper<Page::Page0>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	static constexpr auto upper = (i >> 4) & 0xf;
	static constexpr auto lower = (i >> 0) & 0xf;

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
				AM::Immediate8,	AM::Inherent,	AM::Inherent,	AM::Inherent,
			};
			return s.template schedule<operations[lower], specific_modes[lower]>();
		}
		case 0x2: {
			static constexpr Operation operations[] = {
				O::BRA,		O::BRN,		O::BHI,		O::BLS,		O::BCC,		O::BCS,		O::BNE,		O::BEQ,
				O::BVC,		O::BVS,		O::BPL,		O::BMI,		O::BGE,		O::BLT,		O::BGT,		O::BLE,
			};
			return s.template schedule<operations[lower], AM::Relative8>();
		}
		case 0x3: {
			static constexpr Operation operations[] = {
				O::LEAX,	O::LEAY,	O::LEAS,	O::LEAU,	O::PSHS,	O::PULS,	O::PSHU,	O::PULU,
				O::None,	O::RTS,		O::ABX,		O::RTI,		O::CWAI,	O::MUL,		O::RESET,	O::SWI,
			};
			static constexpr auto op = operations[lower];
			switch(lower) {
				case 0x0:	case 0x1:	case 0x2:	case 0x3:
				return s.template schedule<op, AM::Indexed>();

				case 0x4:	case 0x5:	case 0x6:	case 0x7:	case 0xc:
				return s.template schedule<op, AM::Immediate8>();

				case 0x8:
				return s.template schedule<op, AM::Illegal>();

				default:
				return s.template schedule<op, AM::Inherent>();
			}
		}
		case 0x4: {
			static constexpr Operation operations[] = {
				O::NEGA,	O::None,	O::None,	O::COMA,	O::LSRA,	O::None,	O::RORA,	O::ASRA,
				O::LSLA,	O::ROLA,	O::DECA,	O::None,	O::INCA,	O::TSTA,	O::None,	O::CLRA,
			};
			static constexpr auto op = operations[lower];
			return s.template schedule<op, op == O::None ? AM::Illegal : AM::Inherent>();
		}
		case 0x5: {
			static constexpr Operation operations[] = {
				O::NEGB,	O::None,	O::None,	O::COMB,	O::LSRB,	O::None,	O::RORB,	O::ASRB,
				O::LSLB,	O::ROLB,	O::DECB,	O::None,	O::INCB,	O::TSTB,	O::None,	O::CLRB,
			};
			static constexpr auto op = operations[lower];
			return s.template schedule<op, op == O::None ? AM::Illegal : AM::Inherent>();
		}
		case 0x0: case 0x6:	case 0x7: {
			static constexpr Operation operations[] = {
				O::NEG,		O::None,	O::None,	O::COM,		O::LSR,		O::None,	O::ROR,		O::ASR,
				O::LSL,		O::ROL,		O::DEC,		O::None,	O::INC,		O::TST,		O::JMP,		O::CLR,
			};
			static constexpr auto op = operations[lower];
			return s.template schedule<op, op == O::None ? AM::Illegal : upper == 0 ? AM::Direct : mode<op, i>()>();
		}
		case 0x8:	case 0x9:	case 0xa:	case 0xb: {
			static constexpr Operation operations[] = {
				O::SUBA,	O::CMPA,	O::SBCA,	O::SUBD,	O::ANDA,	O::BITA,	O::LDA,		O::STA,
				O::EORA,	O::ADCA,	O::ORA,		O::ADDA,	O::CMPX,	O::JSR,		O::LDX,		O::STX,
			};
			if(i == 0x8d)	return s.template schedule<O::BSR, AM::Relative8>();
			else			return s.template schedule<operations[lower], mode<operations[lower], i>()>();
		}
		case 0xc:	case 0xd:	case 0xe:	case 0xf: {
			static constexpr Operation operations[] = {
				O::SUBB,	O::CMPB,	O::SBCB,	O::ADDD,	O::ANDB,	O::BITB,	O::LDB,		O::STB,
				O::EORB,	O::ADCB,	O::ORB,		O::ADDB,	O::LDD,		O::STD,		O::LDU,		O::STU,
			};
			return s.template schedule<operations[lower], mode<operations[lower], i>()>();
		}
	}
	__builtin_unreachable();
}

template <>
template <int i, typename SchedulerT>
auto OperationMapper<Page::Page1>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	if constexpr (i >= 0x21 && i < 0x30) {
		static constexpr Operation operations[] = {
						O::LBRN,	O::LBHI,	O::LBLS,	O::LBCC,	O::LBCS,	O::LBNE,	O::LBEQ,
			O::LBVC,	O::LBVS,	O::LBPL,	O::LBMI,	O::LBGE,	O::LBLT,	O::LBGT,	O::LBLE,
		};
		return s.template schedule<operations[i - 0x21], AM::Relative16>();
	} else switch(i) {
		default:	return s.template schedule<O::None, AM::Illegal>();
		case 0x3f:	return s.template schedule<O::SWI2, AM::Inherent>();

		case 0x83:	case 0x93:	case 0xa3:	case 0xb3:
		return s.template schedule<O::CMPD, mode<O::CMPD, i>()>();

		case 0x8c:	case 0x9c:	case 0xac:	case 0xbc:
		return s.template schedule<O::CMPY, mode<O::CMPY, i>()>();

		case 0x8e:	case 0x9e:	case 0xae:	case 0xbe:
		return s.template schedule<O::LDY, mode<O::LDY, i>()>();

		case 0x9f:	case 0xaf:	case 0xbf:
		return s.template schedule<O::STY, mode<O::STY, i>()>();

		case 0xce:	case 0xde:	case 0xee:	case 0xfe:
		return s.template schedule<O::LDS, mode<O::LDS, i>()>();

		case 0xdf:	case 0xef:	case 0xff:
		return s.template schedule<O::STS, mode<O::STS, i>()>();
	}
	__builtin_unreachable();
}

template <>
template <int i, typename SchedulerT>
auto OperationMapper<Page::Page2>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	switch(i) {
		default:	return s.template schedule<O::None, AM::Illegal>();
		case 0x3f:	return s.template schedule<O::SWI3, AM::Inherent>();

		case 0x83:	case 0x93:	case 0xa3:	case 0xb3:
		return s.template schedule<O::CMPU, mode<O::CMPU, i>()>();

		case 0x8c:	case 0x9c:	case 0xac:	case 0xbc:
		return s.template schedule<O::CMPS, mode<O::CMPS, i>()>();
	}
	__builtin_unreachable();
}

}
