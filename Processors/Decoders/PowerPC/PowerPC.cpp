//
//  PowerPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/30/20.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "PowerPC.hpp"

using namespace CPU::Decoder::PowerPC;

// Unmapped:
//
//	absx, clcs, divx, divsx, dozx, dozi, lscbxx, maskgx, maskirx, mulx,
//	nabsx, rlmix, rribx, slex, sleqx, sliqx, slliqx, sllqx, slqx,
//	sraiqx, sraqx, srex, sreax, sreqx, sriqx, srliqx, srlqx, srqx,
//
//	stwcx_,
//
//	frsqrtsx,
//
//	extswx,

Decoder::Decoder(Model model) : model_(model) {}

Instruction Decoder::decode(uint32_t opcode) {
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

#define Bind(mask, operation)				case mask: return Instruction(Operation::operation, opcode);
#define BindSupervisor(mask, operation)		case mask: return Instruction(Operation::operation, opcode, true);
#define BindConditional(condition, mask, operation)	\
	case mask: \
		if(condition()) return Instruction(Operation::operation, opcode);	\
	return Instruction(opcode);
#define BindSupervisorConditional(condition, mask, operation)	\
	case mask: \
		if(condition()) return Instruction(Operation::operation, opcode, true);	\
	return Instruction(opcode);

#define Six(x)			(unsigned(x) << 26)
#define SixTen(x, y)	(Six(x) | (y << 1))

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
		Bind(Six(0b010000), bcx);
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

		// Assumed below here: reserved bits can be ignored.
		// This might need to be a function of CPU model.
		Bind(Six(0b001010), cmpli);		Bind(Six(0b001011), cmpi);
	}
	
	// Second pass: all those with a top six bits and a bottom nine or ten.
	switch(opcode & SixTen(0b111111, 0b1111111111)) {
		default: break;
		
		BindConditional(is64bit, SixTen(0b011111, 0b0000001001), mulhdux);
		BindConditional(is64bit, SixTen(0b011111, 0b0000010101), ldx);
		BindConditional(is64bit, SixTen(0b011111, 0b0000011011), sldx);
		BindConditional(is64bit, SixTen(0b011111, 0b0000110101), ldux);
		BindConditional(is64bit, SixTen(0b011111, 0b0000111010), cntlzdx);
		BindConditional(is64bit, SixTen(0b011111, 0b0001000100), td);
		BindConditional(is64bit, SixTen(0b011111, 0b0001001001), mulhdx);
		BindConditional(is64bit, SixTen(0b011111, 0b0001010100), ldarx);
		BindConditional(is64bit, SixTen(0b011111, 0b0010010101), stdx);
		BindConditional(is64bit, SixTen(0b011111, 0b0010110101), stdux);
		BindConditional(is64bit, SixTen(0b011111, 0b0011101001), mulld);	BindConditional(is64bit, SixTen(0b011111, 0b1011101001), mulld);
		BindConditional(is64bit, SixTen(0b011111, 0b0101010101), lwax);
		BindConditional(is64bit, SixTen(0b011111, 0b0101110101), lwaux);
//		BindConditional(is64bit, SixTen(0b011111, 0b1100111011), sradix);	// TODO: encoding is unclear re: the sh flag.
		BindConditional(is64bit, SixTen(0b011111, 0b0110110010), slbie);
		BindConditional(is64bit, SixTen(0b011111, 0b0111001001), divdux);	BindConditional(is64bit, SixTen(0b011111, 0b1111001001), divdux);
		BindConditional(is64bit, SixTen(0b011111, 0b0111101001), divdx);	BindConditional(is64bit, SixTen(0b011111, 0b1111101001), divdx);
		BindConditional(is64bit, SixTen(0b011111, 0b1000011011), srdx);
		BindConditional(is64bit, SixTen(0b011111, 0b1100011010), sradx);
		BindConditional(is64bit, SixTen(0b011111, 0b1111011010), extsw);

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
		Bind(SixTen(0b011111, 0b0000001011), mulhwux);
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
		Bind(SixTen(0b011111, 0b0001001011), mulhwx);
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

		// Optional...
		Bind(SixTen(0b111111, 0b10110), fsqrtx);
		Bind(SixTen(0b111111, 0b10111), fselx);
		Bind(SixTen(0b111111, 0b11010), frsqrtex);
	}

	// TODO: stwcx., stdcx.		stwcx_

	// Check for sc.
	if((opcode & 0b010001'00000'00000'00000000000000'1'0) == 0b010001'00000'00000'00000000000000'1'0) {
		return Instruction(Operation::sc, opcode);
	}

#undef Six
#undef SixTen

#undef Bind
#undef BindConditional

	return Instruction(opcode);
}
