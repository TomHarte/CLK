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
	switch(opcode >> 26) {
		case 31:
			const uint8_t dest = (opcode >> 21) & 0x1f;
			const uint8_t a = (opcode >> 16) & 0x1f;
			const uint8_t b = (opcode >> 11) & 0x1f;
		
#define OECase(x) case x: case 0x200 + x
			switch((opcode >> 1) & 0x3ff) {
				case 0:
					// cmp; 601 10-26
				break;
				case 4:
					// tw; 601 10-214
				break;
				OECase(8):
					// subfcx; 601 10-207
				break;
//				case 9:
//					// mulhdux
//				break;
				OECase(10):
					// addcx; 601 10-9
				break;
				case 11:
					// mulhwux; 601 10-142
				break;
				case 19:
					// mfcr; 601 10-122
				break;
				case 20:
					// lwarx
				break;
				case 21:
					// ldx
				break;
					// lwzx
					// slwx
					// cntlzwx
					// sldx
					// andx
					// ampl
					// subfx
					// ldux
					// dcbst
					// lwzux
					// cntlzdx
					// andcx
					// td
					// mulhx
					// mulhwx
					// mfmsr
					// ldarx
					// dcbf
					// lbzx
					// negx
					// norx
					// subfex
					// adex
					// mtcrf
					// mtmsr
					// stfx
					// stwcx.
					// stwx
					// stdux
					// stwux
					// subfzex
					// addzex
					// mtsr
					// stdcx.
					// stbx
					// subfmex
					// mulld
					// addmex
					// mullwx
					// mtsrin
					// scbtst
					// stbux
					// addx
					// dcbt
					// lhzx
					// eqvx
					// tlbie
					// eciwx
					// lhzux
					// xorx
					// mfspr
					// lwax
					// lhax
					// lbia
					// mftb
					// lwaux
					// lhaux
					// sthx
					// orcx
					// sradix
					// slbie
					// ecowx
					// sthux
					// orx
					// divdux
					// divwux
					// mtspr
					// dcbi
					// nandx
					// divdx
					// divwx
					// slbia
					// mcrxr
					// lswx
					// lwbrx
					// lfsx
					// srwx
					// srdx
					// tlbsync
					// lfsux
					// mfsr
					// lswi
					// sync
					// lfdx
					// lfdux
					// mfsrin
					// stswx
					// stwbrx
					// stfsx
					// stfsux
					// stswi
					// stfdx
					// stfdux
					// lhbrx
					// srawx
					// sradx
					// srawix
					// eieio
					// ethbrx
					// extshx
					// extsbx
					// lcbi
					// stfiwx
					// extsw
					// dcbz
				OECase(138):
					// addex
				break;
				OECase(266):
					// addx
				break;
				case 28:
					// andx
				break;
				case 60:
					// andcx
				break;
			}
#undef OECase
		break;
	}

	return Instruction();
}
