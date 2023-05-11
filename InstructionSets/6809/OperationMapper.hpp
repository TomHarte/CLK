//
//  OperationMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/03/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M6809_OperationMapper_hpp
#define InstructionSets_M6809_OperationMapper_hpp

// Cf. https://techheap.packetizer.com/processors/6809/6809Instructions.html
//
// Subject to corrections:
//
//	* CWAI and the pushes and pulls at 0x3x are immediate, not inherent.
namespace InstructionSet::M6809 {

enum class AddressingMode {
	Illegal,

	Inherent,
	Immediate,
	Direct,
	Relative,		// TODO: is it worth breaking this into 8- and 16-bit versions?
	Variant,
	Indexed,
	Extended,
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

enum class Page {
	Page0, Page1, Page2,
};

/*!
	Calls @c scheduler.schedule<Operation,AddressingMode> to describe the instruction
	defined by opcode @c i on page @c page.
*/
template <Page page> struct OperationMapper {
	template <int i, typename SchedulerT> void dispatch(SchedulerT &scheduler);
};

template <>
template <int i, typename SchedulerT> void OperationMapper<Page::Page0>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	constexpr auto upper = (i >> 4) & 0xf;
	constexpr auto lower = (i >> 0) & 0xf;

	constexpr AddressingMode modes[] = {
		AM::Immediate, AM::Direct, AM::Indexed, AM::Extended
	};
	constexpr AddressingMode mode = modes[(i >> 4) & 3];

	switch(upper) {
		default: break;

		case 0x1: {
			constexpr Operation operations[] = {
				O::Page1,	O::Page2,	O::NOP,		O::SYNC,	O::None,	O::None,	O::LBRA,	O::LBSR,
				O::None,	O::DAA,		O::ORCC,	O::None,	O::ANDCC,	O::SEX,		O::EXG,		O::TFR,
			};
			constexpr AddressingMode modes[] = {
				AM::Variant,	AM::Variant,	AM::Inherent,	AM::Inherent,
				AM::Illegal,	AM::Illegal,	AM::Relative,	AM::Relative,
				AM::Illegal,	AM::Inherent,	AM::Immediate,	AM::Illegal,
				AM::Immediate, 	AM::Inherent,	AM::Inherent,	AM::Inherent,
			};
			s.template schedule<operations[lower], modes[lower]>();
		} break;
		case 0x2: {
			constexpr Operation operations[] = {
				O::BRA,		O::BRN,		O::BHI,		O::BLS,		O::BCC,		O::BCS,		O::BNE,		O::BEQ,
				O::BVC,		O::BVS,		O::BPL,		O::BMI,		O::BGE,		O::BLT,		O::BGT,		O::BLE,
			};
			s.template schedule<operations[lower], AM::Relative>();
		} break;
		case 0x3: {
			constexpr Operation operations[] = {
				O::LEAX,	O::LEAY,	O::LEAS,	O::LEAU,	O::PSHS,	O::PULS,	O::PSHU,	O::PULU,
				O::None,	O::RTS,		O::ABX,		O::RTI,		O::CWAI,	O::MUL,		O::RESET,	O::SWI,
			};
			constexpr auto op = operations[lower];
			switch(lower) {
				case 0x0:	case 0x1:	case 0x2:	case 0x3:
					s.template schedule<op, AM::Indexed>();
				break;
				case 0x4:	case 0x5:	case 0x6:	case 0x7:	case 0xc:
					s.template schedule<op, AM::Immediate>();
				break;
				case 0x8:
					s.template schedule<op, AM::Illegal>();
				break;
				default:
					s.template schedule<op, AM::Inherent>();
				break;
			}
		} break;
		case 0x4: {
			constexpr Operation operations[] = {
				O::NEGA,	O::None,	O::None,	O::COMA,	O::LSRA,	O::None,	O::RORA,	O::ASRA,
				O::LSLA,	O::ROLA,	O::DECA,	O::None,	O::INCA,	O::TSTA,	O::None,	O::CLRA,
			};
			constexpr auto op = operations[lower];
			s.template schedule<op, op == O::None ? AM::Illegal : AM::Inherent>();
		} break;
		case 0x5: {
			constexpr Operation operations[] = {
				O::NEGB,	O::None,	O::None,	O::COMB,	O::LSRB,	O::None,	O::RORB,	O::ASRB,
				O::LSLB,	O::ROLB,	O::DECB,	O::None,	O::INCB,	O::TSTB,	O::None,	O::CLRB,
			};
			constexpr auto op = operations[lower];
			s.template schedule<op, op == O::None ? AM::Illegal : AM::Inherent>();
		} break;
		case 0x0: case 0x6:	case 0x7: {
			constexpr Operation operations[] = {
				O::NEG,		O::None,	O::None,	O::COM,		O::LSR,		O::None,	O::ROR,		O::ASR,
				O::LSL,		O::ROL,		O::DEC,		O::None,	O::INC,		O::TST,		O::JMP,		O::CLR,
			};
			constexpr auto op = operations[lower];
			s.template schedule<op, op == O::None ? AM::Illegal : upper == 0 ? AM::Direct : mode>();
		} break;
		case 0x8:	case 0x9:	case 0xa:	case 0xb: {
			constexpr Operation operations[] = {
				O::SUBA,	O::CMPA,	O::SBCA,	O::SUBD,	O::ANDA,	O::BITA,	O::LDA,		O::STA,
				O::EORA,	O::ADCA,	O::ORA,		O::ADDA,	O::CMPX,	O::JSR,		O::LDX,		O::STX,
			};
			if(i == 0x8d)	s.template schedule<O::BSR, AM::Relative>();
			else 			s.template schedule<operations[lower], mode>();
		} break;
		case 0xc:	case 0xd:	case 0xe:	case 0xf: {
			constexpr Operation operations[] = {
				O::SUBB,	O::CMPB,	O::SBCB,	O::ADDD,	O::ANDB,	O::BITB,	O::LDB,		O::STB,
				O::EORB,	O::ADCB,	O::ORB,		O::ADDB,	O::LDD,		O::STD,		O::LDU,		O::STU,
			};
			s.template schedule<operations[lower], mode>();
		} break;
	}
}

template <>
template <int i, typename SchedulerT> void OperationMapper<Page::Page1>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	constexpr AddressingMode modes[] = {
		AM::Immediate, AM::Direct, AM::Indexed, AM::Extended
	};
	constexpr auto mode = modes[(i >> 4) & 3];

	if constexpr (i >= 0x21 && i < 0x30) {
		constexpr Operation operations[] = {
						O::LBRN,	O::LBHI,	O::LBLS,	O::LBCC,	O::LBCS,	O::LBNE,	O::LBEQ,
			O::LBVC,	O::LBVS,	O::LBPL,	O::LBMI,	O::LBGE,	O::LBLT,	O::LBGT,	O::LBLE,
		};
		s.template schedule<operations[i - 0x21], AM::Relative>();
	} else switch(i) {
		default:	s.template schedule<O::None, AM::Illegal>();	break;
		case 0x3f:	s.template schedule<O::SWI2, AM::Inherent>();	break;

		case 0x83:	case 0x93:	case 0xa3:	case 0xb3:
			s.template schedule<O::CMPD, mode>();
		break;
		case 0x8c:	case 0x9c:	case 0xac:	case 0xbc:
			s.template schedule<O::CMPY, mode>();
		break;
		case 0x8e:	case 0x9e:	case 0xae:	case 0xbe:
			s.template schedule<O::LDY, mode>();
		break;
		case 0x9f:	case 0xaf:	case 0xbf:
			s.template schedule<O::STY, mode>();
		break;
		case 0xce:	case 0xde:	case 0xee:	case 0xfe:
			s.template schedule<O::LDS, mode>();
		break;
		case 0xdf:	case 0xef:	case 0xff:
			s.template schedule<O::STS, mode>();
		break;
	}
}

template <>
template <int i, typename SchedulerT> void OperationMapper<Page::Page2>::dispatch(SchedulerT &s) {
	using AM = AddressingMode;
	using O = Operation;

	constexpr AddressingMode modes[] = {
		AM::Immediate, AM::Direct, AM::Indexed, AM::Extended
	};
	constexpr auto mode = modes[(i >> 4) & 3];

	switch(i) {
		default:	s.template schedule<O::None, AM::Illegal>();	break;
		case 0x3f:	s.template schedule<O::SWI3, AM::Inherent>();	break;

		case 0x83:	case 0x93:	case 0xa3:	case 0xb3:
			s.template schedule<O::CMPU, mode>();
		break;
		case 0x8c:	case 0x9c:	case 0xac:	case 0xbc:
			s.template schedule<O::CMPS, mode>();
		break;
	}
}

}

#endif /* InstructionSets_M6809_OperationMapper_hpp */
