//
//  PowerPC.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/30/20.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "PowerPC.hpp"

using namespace CPU::Decoder::PowerPC;

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

#define Bind(mask, operation)	case mask: return Instruction(Operation::operation, opcode);
#define BindConditional(condition, mask, operation)	\
	case mask: \
		if(condition()) return Instruction(Operation::operation, opcode);	\
	return Instruction(opcode);

#define Six(x)	(unsigned(x) << 26)

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
	
#undef Six

#undef Bind
#undef BindConditional

	return Instruction(opcode);
}
