//
//  Decoder.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/12/20.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Decoder.hpp"

using namespace InstructionSet::PowerPC;

namespace {

template <Model model, bool validate_reserved_bits, Operation operation> Instruction instruction(uint32_t opcode, bool is_supervisor = false) {
	// If validation isn't required, there's nothing to do here.
	if constexpr (!validate_reserved_bits) {
		return Instruction(operation, opcode, is_supervisor);
	}

	// Otherwise, validation depends on operation
	// (and, in principle, processor model).
	switch(operation) {
		case Operation::absx:		case Operation::clcs:
		case Operation::nabsx:
		case Operation::addmex:		case Operation::addzex:
		case Operation::bcctrx:		case Operation::bclrx:
		case Operation::cntlzdx:	case Operation::cntlzwx:
		case Operation::extsbx:		case Operation::extshx:		case Operation::extswx:
		case Operation::fmulx:		case Operation::fmulsx:
		case Operation::negx:
		case Operation::subfmex:	case Operation::subfzex:
			if(opcode & 0b000000'00000'00000'11111'0000000000'0) return Instruction(opcode);
		break;

		case Operation::cmp:		case Operation::cmpl:
			if(opcode & 0b000000'00010'00000'00000'0000000000'1) return Instruction(opcode);
		break;

		case Operation::cmpi:		case Operation::cmpli:
			if(opcode & 0b000000'00010'00000'00000'0000000000'0) return Instruction(opcode);
		break;

		case Operation::dcbf:		case Operation::dcbi:		case Operation::dcbst:
		case Operation::dcbt:		case Operation::dcbtst:		case Operation::dcbz:
			if(opcode & 0b000000'11111'00000'00000'0000000000'0) return Instruction(opcode);
		break;

		case Operation::crand:		case Operation::crandc:		case Operation::creqv:
		case Operation::crnand:		case Operation::crnor:		case Operation::cror:
		case Operation::crorc:		case Operation::crxor:
		case Operation::eciwx:		case Operation::ecowx:
		case Operation::lbzux:		case Operation::lbzx:
		case Operation::ldarx:
		case Operation::ldux:		case Operation::ldx:
		case Operation::lfdux:		case Operation::lfdx:
		case Operation::lfsux:		case Operation::lfsx:
		case Operation::lhaux:		case Operation::lhax:		case Operation::lhbrx:
		case Operation::lhzux:		case Operation::lhzx:
		case Operation::lswi:		case Operation::lswx:
		case Operation::lwarx:		case Operation::lwaux:		case Operation::lwax:		case Operation::lwbrx:
		case Operation::lwzux:		case Operation::lwzx:
		case Operation::mfspr:		case Operation::mftb:
		case Operation::mtspr:
		case Operation::stbux:		case Operation::stbx:
		case Operation::stdux:		case Operation::stdx:
		case Operation::stfdux:		case Operation::stfdx:
		case Operation::stfiwx:
		case Operation::stfsux:		case Operation::stfsx:
		case Operation::sthbrx:
		case Operation::sthux:		case Operation::sthx:
		case Operation::stswi:		case Operation::stswx:
		case Operation::stwbrx:
		case Operation::stwux:		case Operation::stwx:
		case Operation::td:			case Operation::tw:
			if(opcode & 0b000000'00000'00000'00000'0000000000'1) return Instruction(opcode);
		break;

		case Operation::fabsx:		case Operation::fcfidx:
		case Operation::fctidx:		case Operation::fctidzx:
		case Operation::fctiwx:		case Operation::fctiwzx:
		case Operation::fmrx:		case Operation::fnabsx:
		case Operation::fnegx:		case Operation::frspx:
			if(opcode & 0b000000'00000'11111'00000'0000000000'0) return Instruction(opcode);
		break;

		case Operation::faddx:		case Operation::faddsx:
		case Operation::fdivx:		case Operation::fdivsx:
		case Operation::fsubx:		case Operation::fsubsx:
			if(opcode & 0b000000'00000'00000'00000'1111100000'0) return Instruction(opcode);
		break;

		case Operation::fcmpo:		case Operation::fcmpu:
			if(opcode & 0b000000'00011'00000'00000'0000000000'1) return Instruction(opcode);
		break;

		case Operation::fresx:		case Operation::frsqrtex:
		case Operation::fsqrtx:		case Operation::fsqrtsx:
			if(opcode & 0b000000'00000'11111'00000'1111100000'1) return Instruction(opcode);
		break;

		case Operation::icbi:
			if(opcode & 0b000000'11111'00000'00000'0000000000'1) return Instruction(opcode);
		break;

		case Operation::eieio:
		case Operation::isync:
		case Operation::rfi:
		case Operation::slbia:
		case Operation::sync:
		case Operation::tlbia:
		case Operation::tlbsync:
			if(opcode & 0b000000'11111'11111'11111'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mcrf:		case Operation::mcrfs:
			if(opcode & 0b000000'00011'00011'11111'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mcrxr:
			if(opcode & 0b000000'00011'11111'11111'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mfcr:
		case Operation::mfmsr:
		case Operation::mtmsr:
			if(opcode & 0b000000'00000'11111'11111'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mffsx:
		case Operation::mtfsb0x:
		case Operation::mtfsb1x:
			if(opcode & 0b000000'00000'11111'11111'0000000000'0) return Instruction(opcode);
		break;

		case Operation::mtfsfx:
			if(opcode & 0b000000'10000'00001'00000'0000000000'0) return Instruction(opcode);
		break;

		case Operation::mtfsfix:
			if(opcode & 0b000000'00011'11111'00001'0000000000'0) return Instruction(opcode);
		break;

		case Operation::mtsr:
			if(opcode & 0b000000'00000'10000'11111'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mtsrin:		case Operation::mfsrin:
			if(opcode & 0b000000'00000'11111'00000'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mfsr:
			if(opcode & 0b000000'00000'10000'11111'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mtcrf:
			if(opcode & 0b000000'00000'10000'00001'0000000000'1) return Instruction(opcode);
		break;

		case Operation::mulhdx:		case Operation::mulhdux:
		case Operation::mulhwx:		case Operation::mulhwux:
			if(opcode & 0b000000'00000'00000'00000'1000000000'0) return Instruction(opcode);
		break;

		case Operation::sc:
			if(opcode & 0b000000'11111'11111'11111'1111111110'1) return Instruction(opcode);
		break;

		case Operation::slbie:
		case Operation::tlbie:
			if(opcode & 0b000000'11111'11111'00000'0000000000'1) return Instruction(opcode);
		break;

		case Operation::stwcx_:
			if(!(opcode & 0b000000'00000'00000'00000'0000000000'1)) return Instruction(opcode);
		break;

		case Operation::divx:		case Operation::divsx:
		case Operation::dozx:		case Operation::dozi:
		case Operation::lscbxx:
		case Operation::maskgx:		case Operation::maskirx:
		case Operation::mulx:
		case Operation::rlmix:		case Operation::rribx:
		case Operation::slex:		case Operation::sleqx:		case Operation::sliqx:
		case Operation::slliqx:		case Operation::sllqx:		case Operation::slqx:
		case Operation::sraiqx:		case Operation::sraqx:
		case Operation::srex:		case Operation::sreqx:
		case Operation::sriqx:		case Operation::srliqx:
		case Operation::srlqx:		case Operation::srqx:
		case Operation::sreax:
		case Operation::addx:		case Operation::addcx:		case Operation::addex:
		case Operation::addi:		case Operation::addic:		case Operation::addic_:
		case Operation::addis:
		case Operation::andx:		case Operation::andcx:
		case Operation::andi_:		case Operation::andis_:
		case Operation::bx:			case Operation::bcx:
		case Operation::divdx:		case Operation::divdux:
		case Operation::divwx:		case Operation::divwux:
		case Operation::eqvx:
		case Operation::fmaddx:		case Operation::fmaddsx:
		case Operation::fmsubx:		case Operation::fmsubsx:
		case Operation::fnmaddx:	case Operation::fnmaddsx:
		case Operation::fnmsubx:	case Operation::fnmsubsx:
		case Operation::fselx:
		case Operation::lbz:		case Operation::lbzu:
		case Operation::lfd:		case Operation::lfdu:
		case Operation::lfs:		case Operation::lfsu:
		case Operation::lha:		case Operation::lhau:
		case Operation::lhz:		case Operation::lhzu:
		case Operation::lmw:		case Operation::lwa:
		case Operation::lwz:		case Operation::lwzu:
		case Operation::mulldx:		case Operation::mulli:		case Operation::mullwx:
		case Operation::nandx:		case Operation::norx:
		case Operation::orx:		case Operation::orcx:
		case Operation::ori:		case Operation::oris:
		case Operation::rlwimix:	case Operation::rlwinmx:	case Operation::rlwnmx:
		case Operation::sldx:		case Operation::slwx:
		case Operation::sradx:		case Operation::sradix:
		case Operation::srawx:		case Operation::srawix:
		case Operation::srdx:		case Operation::srwx:
		case Operation::stb:		case Operation::stbu:
		case Operation::std:		case Operation::stdcx_:		case Operation::stdu:
		case Operation::stfd:		case Operation::stfdu:
		case Operation::stfs:		case Operation::stfsu:
		case Operation::sth:		case Operation::sthu:
		case Operation::stmw:
		case Operation::stw:		case Operation::stwu:
		case Operation::subfx:		case Operation::subfcx:		case Operation::subfex:
		case Operation::subfic:
		case Operation::tdi:		case Operation::twi:
		case Operation::xorx:		case Operation::xori:		case Operation::xoris:
		case Operation::ld:			case Operation::ldu:
		case Operation::rldclx:		case Operation::rldcrx:
		case Operation::rldicx:		case Operation::rldiclx:
		case Operation::rldicrx:	case Operation::rldimix:

		break;
	}

	return Instruction(operation, opcode, is_supervisor);
}

}

template <Model model, bool validate_reserved_bits>
Instruction Decoder<model, validate_reserved_bits>::decode(uint32_t opcode) {
	// Quick bluffer's guide to PowerPC instruction encoding:
	//
	// There is a six-bit field at the very top of the instruction.
	// Sometimes that fully identifies an instruction, but usually
	// it doesn't.
	//
	// There is an addition 9- or 10-bit field starting one bit above
	// least significant that disambiguates the rest. Strictly speaking
	// it's a 10-bit field, but the mnemonics for many instructions treat
	// it as a 9-bit field with a flag at the top.
	//
	// I've decided to hew directly to the mnemonics.
	//
	// Various opcodes in the 1995 documentation define reserved bits,
	// which are given the nominal value of 0. It does not give a formal
	// definition of a reserved bit. As a result this code does not
	// currently check the value of reserved bits. That may need to change
	// if/when I add support for extended instruction sets.

#define Bind(mask, operation)				case mask: return instruction<model, validate_reserved_bits, Operation::operation>(opcode);
#define BindSupervisor(mask, operation)		case mask: return instruction<model, validate_reserved_bits, Operation::operation>(opcode, true);
#define BindConditional(condition, mask, operation)	\
	case mask: \
		if(condition(model)) return instruction<model, validate_reserved_bits, Operation::operation>(opcode);	\
	return instruction<model, validate_reserved_bits, Operation::operation>(opcode);
#define BindSupervisorConditional(condition, mask, operation)	\
	case mask: \
		if(condition(model)) return instruction<model, validate_reserved_bits, Operation::operation>(opcode, true);	\
	return instruction<model, validate_reserved_bits, Operation::operation>(opcode);

#define Six(x)			(unsigned(x) << 26)
#define SixTen(x, y)	(Six(x) | ((y) << 1))

	// First pass: weed out all those instructions identified entirely by the
	// top six bits.
	switch(opcode & Six(0b111111)) {
		default: break;

		BindConditional(is64bit, Six(0b000010), tdi);

		Bind(Six(0b000011), twi);
		Bind(Six(0b000111), mulli);
		Bind(Six(0b001000), subfic);
		Bind(Six(0b001100), addic);		Bind(Six(0b001101), addic_);
		Bind(Six(0b001110), addi);		Bind(Six(0b001111), addis);
		case Six(0b010000): {
			// This might be a bcx, but check for a valid bo field.
			switch((opcode >> 21) & 0x1f) {
				case 0: case 1: case 2: case 3: case 4: case 5:
				case 8: case 9: case 10: case 11: case 12: case 13:
				case 16: case 17: case 18: case 19: case 20:
				return instruction<model, validate_reserved_bits, Operation::bcx>(opcode);

				default: return Instruction(opcode);
			}
		} break;
		Bind(Six(0b010010), bx);
		Bind(Six(0b010100), rlwimix);
		Bind(Six(0b010101), rlwinmx);
		Bind(Six(0b010111), rlwnmx);

		Bind(Six(0b011000), ori);		Bind(Six(0b011001), oris);
		Bind(Six(0b011010), xori);		Bind(Six(0b011011), xoris);
		Bind(Six(0b011100), andi_);		Bind(Six(0b011101), andis_);
		Bind(Six(0b100000), lwz);		Bind(Six(0b100001), lwzu);
		Bind(Six(0b100010), lbz);		Bind(Six(0b100011), lbzu);
		Bind(Six(0b100100), stw);		Bind(Six(0b100101), stwu);
		Bind(Six(0b100110), stb);		Bind(Six(0b100111), stbu);
		Bind(Six(0b101000), lhz);		Bind(Six(0b101001), lhzu);
		Bind(Six(0b101010), lha);		Bind(Six(0b101011), lhau);
		Bind(Six(0b101100), sth);		Bind(Six(0b101101), sthu);
		Bind(Six(0b101110), lmw);		Bind(Six(0b101111), stmw);
		Bind(Six(0b110000), lfs);		Bind(Six(0b110001), lfsu);
		Bind(Six(0b110010), lfd);		Bind(Six(0b110011), lfdu);
		Bind(Six(0b110100), stfs);		Bind(Six(0b110101), stfsu);
		Bind(Six(0b110110), stfd);		Bind(Six(0b110111), stfdu);

		BindConditional(is601, Six(9), dozi);
		BindConditional(is601, Six(22), rlmix);

		Bind(Six(0b001010), cmpli);		Bind(Six(0b001011), cmpi);
	}
	
	// Second pass: all those with a top six bits and a bottom nine or ten.
	switch(opcode & SixTen(0b111111, 0b1111111111)) {
		default: break;

		// 64-bit instructions.
		BindConditional(is64bit, SixTen(0b011111, 0b0000001001), mulhdux);	BindConditional(is64bit, SixTen(0b011111, 0b1000001001), mulhdux);
		BindConditional(is64bit, SixTen(0b011111, 0b0000010101), ldx);
		BindConditional(is64bit, SixTen(0b011111, 0b0000011011), sldx);
		BindConditional(is64bit, SixTen(0b011111, 0b0000110101), ldux);
		BindConditional(is64bit, SixTen(0b011111, 0b0000111010), cntlzdx);
		BindConditional(is64bit, SixTen(0b011111, 0b0001000100), td);
		BindConditional(is64bit, SixTen(0b011111, 0b0001001001), mulhdx);	BindConditional(is64bit, SixTen(0b011111, 0b1001001001), mulhdx);
		BindConditional(is64bit, SixTen(0b011111, 0b0001010100), ldarx);
		BindConditional(is64bit, SixTen(0b011111, 0b0010010101), stdx);
		BindConditional(is64bit, SixTen(0b011111, 0b0010110101), stdux);
		BindConditional(is64bit, SixTen(0b011111, 0b0011101001), mulldx);	BindConditional(is64bit, SixTen(0b011111, 0b1011101001), mulldx);
		BindConditional(is64bit, SixTen(0b011111, 0b0101010101), lwax);
		BindConditional(is64bit, SixTen(0b011111, 0b0101110101), lwaux);
		BindConditional(is64bit, SixTen(0b011111, 0b1100111011), sradix);	BindConditional(is64bit, SixTen(0b011111, 0b1100111010), sradix);
		BindConditional(is64bit, SixTen(0b011111, 0b0110110010), slbie);
		BindConditional(is64bit, SixTen(0b011111, 0b0111001001), divdux);	BindConditional(is64bit, SixTen(0b011111, 0b1111001001), divdux);
		BindConditional(is64bit, SixTen(0b011111, 0b0111101001), divdx);	BindConditional(is64bit, SixTen(0b011111, 0b1111101001), divdx);
		BindConditional(is64bit, SixTen(0b011111, 0b1000011011), srdx);
		BindConditional(is64bit, SixTen(0b011111, 0b1100011010), sradx);
		BindConditional(is64bit, SixTen(0b111111, 0b1111011010), extswx);

		// Power instructions; these are all taken from the MPC601 manual rather than
		// the PowerPC Programmer's Reference Guide, hence the decimal encoding of the
		// ten-bit field.
		BindConditional(is601, SixTen(0b011111, 360), absx);	BindConditional(is601, SixTen(0b011111, 512 + 360), absx);
		BindConditional(is601, SixTen(0b011111, 531), clcs);
		BindConditional(is601, SixTen(0b011111, 331), divx);	BindConditional(is601, SixTen(0b011111, 512 + 331), divx);
		BindConditional(is601, SixTen(0b011111, 363), divsx);	BindConditional(is601, SixTen(0b011111, 512 + 363), divsx);
		BindConditional(is601, SixTen(0b011111, 264), dozx);	BindConditional(is601, SixTen(0b011111, 512 + 264), dozx);
		BindConditional(is601, SixTen(0b011111, 277), lscbxx);
		BindConditional(is601, SixTen(0b011111, 29), maskgx);
		BindConditional(is601, SixTen(0b011111, 541), maskirx);
		BindConditional(is601, SixTen(0b011111, 107), mulx);	BindConditional(is601, SixTen(0b011111, 512 + 107), mulx);
		BindConditional(is601, SixTen(0b011111, 488), nabsx);	BindConditional(is601, SixTen(0b011111, 512 + 488), nabsx);
		BindConditional(is601, SixTen(0b011111, 537), rribx);
		BindConditional(is601, SixTen(0b011111, 153), slex);
		BindConditional(is601, SixTen(0b011111, 217), sleqx);
		BindConditional(is601, SixTen(0b011111, 184), sliqx);
		BindConditional(is601, SixTen(0b011111, 248), slliqx);
		BindConditional(is601, SixTen(0b011111, 216), sllqx);
		BindConditional(is601, SixTen(0b011111, 152), slqx);
		BindConditional(is601, SixTen(0b011111, 952), sraiqx);
		BindConditional(is601, SixTen(0b011111, 920), sraqx);
		BindConditional(is601, SixTen(0b011111, 665), srex);
		BindConditional(is601, SixTen(0b011111, 921), sreax);
		BindConditional(is601, SixTen(0b011111, 729), sreqx);
		BindConditional(is601, SixTen(0b011111, 696), sriqx);
		BindConditional(is601, SixTen(0b011111, 760), srliqx);
		BindConditional(is601, SixTen(0b011111, 728), srlqx);
		BindConditional(is601, SixTen(0b011111, 664), srqx);

		// 32-bit instructions.
		Bind(SixTen(0b010011, 0b0000000000), mcrf);
		Bind(SixTen(0b010011, 0b0000010000), bclrx);
		Bind(SixTen(0b010011, 0b0000100001), crnor);
		Bind(SixTen(0b010011, 0b0000110010), rfi);
		Bind(SixTen(0b010011, 0b0010000001), crandc);
		Bind(SixTen(0b010011, 0b0010010110), isync);
		Bind(SixTen(0b010011, 0b0011000001), crxor);
		Bind(SixTen(0b010011, 0b0011100001), crnand);
		Bind(SixTen(0b010011, 0b0100000001), crand);
		Bind(SixTen(0b010011, 0b0100100001), creqv);
		Bind(SixTen(0b010011, 0b0110100001), crorc);
		Bind(SixTen(0b010011, 0b0111000001), cror);
		Bind(SixTen(0b010011, 0b1000010000), bcctrx);
		Bind(SixTen(0b011111, 0b0000000000), cmp);
		Bind(SixTen(0b011111, 0b0000000100), tw);
		Bind(SixTen(0b011111, 0b0000001000), subfcx);	Bind(SixTen(0b011111, 0b1000001000), subfcx);
		Bind(SixTen(0b011111, 0b0000001010), addcx);	Bind(SixTen(0b011111, 0b1000001010), addcx);
		Bind(SixTen(0b011111, 0b0000001011), mulhwux);	Bind(SixTen(0b011111, 0b1000001011), mulhwux);
		Bind(SixTen(0b011111, 0b0000010011), mfcr);
		Bind(SixTen(0b011111, 0b0000010100), lwarx);
		Bind(SixTen(0b011111, 0b0000010111), lwzx);
		Bind(SixTen(0b011111, 0b0000011000), slwx);
		Bind(SixTen(0b011111, 0b0000011010), cntlzwx);
		Bind(SixTen(0b011111, 0b0000011100), andx);
		Bind(SixTen(0b011111, 0b0000100000), cmpl);
		Bind(SixTen(0b011111, 0b0000101000), subfx);	Bind(SixTen(0b011111, 0b1000101000), subfx);
		Bind(SixTen(0b011111, 0b0000110110), dcbst);
		Bind(SixTen(0b011111, 0b0000110111), lwzux);
		Bind(SixTen(0b011111, 0b0000111100), andcx);
		Bind(SixTen(0b011111, 0b0001001011), mulhwx);	Bind(SixTen(0b011111, 0b1001001011), mulhwx);
		Bind(SixTen(0b011111, 0b0001010011), mfmsr);
		Bind(SixTen(0b011111, 0b0001010110), dcbf);
		Bind(SixTen(0b011111, 0b0001010111), lbzx);
		Bind(SixTen(0b011111, 0b0001101000), negx);		Bind(SixTen(0b011111, 0b1001101000), negx);
		Bind(SixTen(0b011111, 0b0001110111), lbzux);
		Bind(SixTen(0b011111, 0b0001111100), norx);
		Bind(SixTen(0b011111, 0b0010001000), subfex);	Bind(SixTen(0b011111, 0b1010001000), subfex);
		Bind(SixTen(0b011111, 0b0010001010), addex);	Bind(SixTen(0b011111, 0b1010001010), addex);
		Bind(SixTen(0b011111, 0b0010010000), mtcrf);
		Bind(SixTen(0b011111, 0b0010010010), mtmsr);
		Bind(SixTen(0b011111, 0b0010010111), stwx);
		Bind(SixTen(0b011111, 0b0010110111), stwux);
		Bind(SixTen(0b011111, 0b0011001000), subfzex);	Bind(SixTen(0b011111, 0b1011001000), subfzex);
		Bind(SixTen(0b011111, 0b0011001010), addzex);	Bind(SixTen(0b011111, 0b1011001010), addzex);
		Bind(SixTen(0b011111, 0b0011010111), stbx);
		Bind(SixTen(0b011111, 0b0011101000), subfmex);	Bind(SixTen(0b011111, 0b1011101000), subfmex);
		Bind(SixTen(0b011111, 0b0011101010), addmex);	Bind(SixTen(0b011111, 0b1011101010), addmex);
		Bind(SixTen(0b011111, 0b0011101011), mullwx);	Bind(SixTen(0b011111, 0b1011101011), mullwx);
		Bind(SixTen(0b011111, 0b0011110110), dcbtst);
		Bind(SixTen(0b011111, 0b0011110111), stbux);
		Bind(SixTen(0b011111, 0b0100001010), addx);		Bind(SixTen(0b011111, 0b1100001010), addx);
		Bind(SixTen(0b011111, 0b0100010110), dcbt);
		Bind(SixTen(0b011111, 0b0100010111), lhzx);
		Bind(SixTen(0b011111, 0b0100011100), eqvx);
		Bind(SixTen(0b011111, 0b0100110110), eciwx);
		Bind(SixTen(0b011111, 0b0100110111), lhzux);
		Bind(SixTen(0b011111, 0b0100111100), xorx);
		Bind(SixTen(0b011111, 0b0101010111), lhax);
		Bind(SixTen(0b011111, 0b0101110011), mftb);
		Bind(SixTen(0b011111, 0b0101110111), lhaux);
		Bind(SixTen(0b011111, 0b0110010111), sthx);
		Bind(SixTen(0b011111, 0b0110011100), orcx);
		Bind(SixTen(0b011111, 0b0110110110), ecowx);
		Bind(SixTen(0b011111, 0b0110110111), sthux);
		Bind(SixTen(0b011111, 0b0110111100), orx);
		Bind(SixTen(0b011111, 0b0111001011), divwux);		Bind(SixTen(0b011111, 0b1111001011), divwux);
		Bind(SixTen(0b011111, 0b0111010110), dcbi);
		Bind(SixTen(0b011111, 0b0111011100), nandx);
		Bind(SixTen(0b011111, 0b0111101011), divwx);		Bind(SixTen(0b011111, 0b1111101011), divwx);
		Bind(SixTen(0b011111, 0b1000000000), mcrxr);
		Bind(SixTen(0b011111, 0b1000010101), lswx);
		Bind(SixTen(0b011111, 0b1000010110), lwbrx);
		Bind(SixTen(0b011111, 0b1000010111), lfsx);
		Bind(SixTen(0b011111, 0b1000011000), srwx);
		Bind(SixTen(0b011111, 0b1000110111), lfsux);
		Bind(SixTen(0b011111, 0b1001010101), lswi);
		Bind(SixTen(0b011111, 0b1001010110), sync);
		Bind(SixTen(0b011111, 0b1001010111), lfdx);
		Bind(SixTen(0b011111, 0b1001110111), lfdux);
		Bind(SixTen(0b011111, 0b1010010101), stswx);
		Bind(SixTen(0b011111, 0b1010010110), stwbrx);
		Bind(SixTen(0b011111, 0b1010010111), stfsx);
		Bind(SixTen(0b011111, 0b1010110111), stfsux);
		Bind(SixTen(0b011111, 0b1011010101), stswi);
		Bind(SixTen(0b011111, 0b1011010111), stfdx);
		Bind(SixTen(0b011111, 0b1011110111), stfdux);
		Bind(SixTen(0b011111, 0b1100010110), lhbrx);
		Bind(SixTen(0b011111, 0b1100011000), srawx);
		Bind(SixTen(0b011111, 0b1100111000), srawix);
		Bind(SixTen(0b011111, 0b1101010110), eieio);
		Bind(SixTen(0b011111, 0b1110010110), sthbrx);
		Bind(SixTen(0b011111, 0b1110011010), extshx);
		Bind(SixTen(0b011111, 0b1110111010), extsbx);
		Bind(SixTen(0b011111, 0b1111010110), icbi);
		Bind(SixTen(0b011111, 0b1111010111), stfiwx);
		Bind(SixTen(0b011111, 0b1111110110), dcbz);
		Bind(SixTen(0b111111, 0b0000000000), fcmpu);
		Bind(SixTen(0b111111, 0b0000001100), frspx);
		Bind(SixTen(0b111111, 0b0000001110), fctiwx);
		Bind(SixTen(0b111111, 0b0000001111), fctiwzx);
		Bind(SixTen(0b111111, 0b0000100000), fcmpo);
		Bind(SixTen(0b111111, 0b0000100110), mtfsb1x);
		Bind(SixTen(0b111111, 0b0000101000), fnegx);
		Bind(SixTen(0b111111, 0b0001000000), mcrfs);
		Bind(SixTen(0b111111, 0b0001000110), mtfsb0x);
		Bind(SixTen(0b111111, 0b0001001000), fmrx);
		Bind(SixTen(0b111111, 0b0010000110), mtfsfix);
		Bind(SixTen(0b111111, 0b0010001000), fnabsx);
		Bind(SixTen(0b111111, 0b0100001000), fabsx);
		Bind(SixTen(0b111111, 0b1001000111), mffsx);
		Bind(SixTen(0b111111, 0b1011000111), mtfsfx);
		Bind(SixTen(0b111111, 0b1100101110), fctidx);
		Bind(SixTen(0b111111, 0b1100101111), fctidzx);
		Bind(SixTen(0b111111, 0b1101001110), fcfidx);

		Bind(SixTen(0b011111, 0b0101010011), mfspr);	// Flagged as "supervisor and user"?
		Bind(SixTen(0b011111, 0b0111010011), mtspr);	// Flagged as "supervisor and user"?

		BindSupervisorConditional(is32bit, SixTen(0b011111, 0b0011010010), mtsr);
		BindSupervisorConditional(is32bit, SixTen(0b011111, 0b0011110010), mtsrin);
		BindSupervisorConditional(is32bit, SixTen(0b011111, 0b1001010011), mfsr);
		BindSupervisorConditional(is32bit, SixTen(0b011111, 0b1010010011), mfsrin);

		BindSupervisorConditional(is64bit, SixTen(0b011111, 0b0111110010), slbia);	// optional

		// The following are all optional; should I record that?
		BindSupervisor(SixTen(0b011111, 0b0100110010), tlbie);
		BindSupervisor(SixTen(0b011111, 0b0101110010), tlbia);
		BindSupervisor(SixTen(0b011111, 0b1000110110), tlbsync);
	}

	// Third pass: like six-ten except that the top five of the final ten
	// are reserved (i.e. ignored here).
	switch(opcode & SixTen(0b111111, 0b11111)) {
		default: break;

		Bind(SixTen(0b111011, 0b10010), fdivsx);
		Bind(SixTen(0b111011, 0b10100), fsubsx);
		Bind(SixTen(0b111011, 0b10101), faddsx);
		Bind(SixTen(0b111011, 0b11001), fmulsx);
		Bind(SixTen(0b111011, 0b11100), fmsubsx);
		Bind(SixTen(0b111011, 0b11101), fmaddsx);
		Bind(SixTen(0b111011, 0b11110), fnmsubsx);
		Bind(SixTen(0b111011, 0b11111), fnmaddsx);

		Bind(SixTen(0b111111, 0b10010), fdivx);
		Bind(SixTen(0b111111, 0b10100), fsubx);
		Bind(SixTen(0b111111, 0b10101), faddx);
		Bind(SixTen(0b111111, 0b11001), fmulx);
		Bind(SixTen(0b111111, 0b11100), fmsubx);
		Bind(SixTen(0b111111, 0b11101), fmaddx);
		Bind(SixTen(0b111111, 0b11110), fnmsubx);
		Bind(SixTen(0b111111, 0b11111), fnmaddx);

		BindConditional(is64bit, SixTen(0b111011, 0b10110), fsqrtsx);
		BindConditional(is64bit, SixTen(0b111011, 0b11000), fresx);
		BindConditional(is64bit, SixTen(0b011110, 0b01000), rldclx);
		BindConditional(is64bit, SixTen(0b011110, 0b01001), rldcrx);

		// Optional...
		Bind(SixTen(0b111111, 0b10110), fsqrtx);
		Bind(SixTen(0b111111, 0b10111), fselx);
		Bind(SixTen(0b111111, 0b11010), frsqrtex);
	}

	// rldicx, rldiclx, rldicrx, rldimix
	if(is64bit(model)) {
		switch(opcode & 0b111111'00000'00000'00000'000000'111'00) {
			default: break;
			case 0b011110'00000'00000'00000'000000'000'00:	return instruction<model, validate_reserved_bits, Operation::rldiclx>(opcode);
			case 0b011110'00000'00000'00000'000000'001'00:	return instruction<model, validate_reserved_bits, Operation::rldicrx>(opcode);
			case 0b011110'00000'00000'00000'000000'010'00:	return instruction<model, validate_reserved_bits, Operation::rldicx>(opcode);
			case 0b011110'00000'00000'00000'000000'011'00:	return instruction<model, validate_reserved_bits, Operation::rldimix>(opcode);
		}
	}

	// stwcx. and stdcx.
	switch(opcode & 0b111111'0000'0000'0000'0000'111111111'1) {
		default: break;
		case 0b011111'0000'0000'0000'0000'010010110'1:	return instruction<model, validate_reserved_bits, Operation::stwcx_>(opcode);
		case 0b011111'0000'0000'0000'0000'011010110'1:
			if(is64bit(model)) return instruction<model, validate_reserved_bits, Operation::stdcx_>(opcode);
		return Instruction(opcode);
	}

	// std, stdu, ld, ldu, lwa
	if(is64bit(model)) {
		switch(opcode & 0b111111'00'00000000'00000000'000000'11) {
			default: break;
			case 0b111010'00'00000000'00000000'000000'00:	return instruction<model, validate_reserved_bits, Operation::ld>(opcode);
			case 0b111010'00'00000000'00000000'000000'01:	return instruction<model, validate_reserved_bits, Operation::ldu>(opcode);
			case 0b111010'00'00000000'00000000'000000'10:	return instruction<model, validate_reserved_bits, Operation::lwa>(opcode);
			case 0b111110'00'00000000'00000000'000000'00:	return instruction<model, validate_reserved_bits, Operation::std>(opcode);
			case 0b111110'00'00000000'00000000'000000'01:	return instruction<model, validate_reserved_bits, Operation::stdu>(opcode);
		}
	}

	// sc
	if((opcode & 0b111111'00'00000000'00000000'000000'1'0) == 0b010001'00'00000000'00000000'000000'1'0) {
		return instruction<model, validate_reserved_bits, Operation::sc>(opcode);
	}

#undef Six
#undef SixTen

#undef Bind
#undef BindConditional

	return Instruction(opcode);
}

template class InstructionSet::PowerPC::Decoder<InstructionSet::PowerPC::Model::MPC601, true>;
template class InstructionSet::PowerPC::Decoder<InstructionSet::PowerPC::Model::MPC603, true>;
template class InstructionSet::PowerPC::Decoder<InstructionSet::PowerPC::Model::MPC620, true>;

template class InstructionSet::PowerPC::Decoder<InstructionSet::PowerPC::Model::MPC601, false>;
template class InstructionSet::PowerPC::Decoder<InstructionSet::PowerPC::Model::MPC603, false>;
template class InstructionSet::PowerPC::Decoder<InstructionSet::PowerPC::Model::MPC620, false>;
