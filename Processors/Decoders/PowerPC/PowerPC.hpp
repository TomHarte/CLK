//
//  PowerPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/30/20.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef PowerPC_hpp
#define PowerPC_hpp

#include <cstdint>

namespace CPU {
namespace Decoder {
namespace PowerPC {

enum class Model {
	MPC601,
};

enum class Operation: uint8_t {
	Undefined,

	// These 601-exclusive instructions; a lot of them are carry-overs
	// from POWER.
	absx, clcs, divx, divsx, dozx, dozi, lscbxx, maskgx, maskirx, mulx,
	nabsx, rlmix, rribx, slex, sleqx, sliqx, slliqx, sllqx, slqx,
	sraiqx, sraqx, srex, sreax, sreqx, sriqx, srliqx, srlqx, srqx,
	
	// 32- and 64-bit PowerPC instructions.
	addx, addcx, addex, addi, addic, addic_, addis, addmex, addzex, andx,
	andcx, andi_, andis_, bx, bcx, bcctrx, bclrx, cmp, cmpi, cmpl, cmpli,
	cntlzwx, crand, crandc, creqv, crnand, crnor, cror, crorc, crxor, dcbf,
	dcbst, dcbt, dcbtst, dcbz, divwx, divwux, eciwx, ecowx, eieio, eqvx,
	extsbx, extshx, fabsx, faddx, faddsx, fcmpo, fcmpu, fctiwx, fctiwzx,
	fdivx, fdivsx, fmaddx, fmaddsx, fmrx, fmsubx, fmsubsx, fmulx, fmulsx,
	fnabsx, fnegx, fnmaddx, fnmaddsx, fnmsubx, fnmsubsx, frspx, fsubx, fsubsx,
	icbi, isync, lbz, lbzu, lbzux, lbzx, lfd, lfdu, lfdux, lfdx, lfs, lfsu,
	lfsux, lfsx, lha, lhau, lhaux, lhax, lhbrx, lhz, lhzu, lhzux, lhzx, lmw,
	lswi, lswx, lwarx, lwbrx, lwz, lwzu, lwzux, lwzx, mcrf, mcrfs, mcrxr,
	mfcr, mffsx, mfmsr, mfspr, mfsr, mfsrin, mtcrf, mtfsb0x, mtfsb1x, mtfsfx,
	mtfsfix, mtmsr, mtspr, mtsr, mtsrin, mulhwx, mulhwux, mulli, mullwx,
	nandx, negx, norx, orx, orcx, ori, oris, rfi, rlwimix, rlwinmx, rlwnmx,
	sc, slwx, srawx, srawix, srwx, stb, stbu, stbux, stbx, stfd, stfdu,
	stfdux, stfdx, stfs, stfsu, stfsux, stfsx, sth, sthbrx, sthu, sthux, sthx,
	stmw, stswi, stswx, stw, stwbrx, stwcx_, stwu, stwux, stwx, subfx, subfcx,
	subfex, subfic, subfmex, subfzex, sync, tw, twi, xorx, xori, xoris, mftb,

	// 32-bit, supervisor level.
	dcbi,

	// Supervisor, optional.
	tlbia, tlbie, tlbsync,

	// Optional.
	fresx, frsqrtex, fselx, fsqrtx, frsqrtsx, slbia, slbie, stfiwx,

	// 64-bit only PowerPC instructions.
	cntlzdx, divdx, divdux, extswx, fcfidx, fctidx, fctidzx, tdi, mulhdux,
	ldx, sldx, ldux, td, mulhdx, ldarx, stdx, stdux, mulld, lwax, lwaux,
	sradix, srdx, sradx, extsw, fsqrtsx
};

/*!
	Holds a decoded PowerPC instruction.

	Implementation note: because the PowerPC encoding is particularly straightforward,
	only the operation has been decoded ahead of time; all other fields are decoded on-demand.
*/
struct Instruction {
	const Operation operation = Operation::Undefined;
	const bool is_supervisor = false;
	const uint32_t opcode = 0;

	Instruction(uint32_t opcode) : opcode(opcode) {}
	Instruction(Operation operation, uint32_t opcode, bool is_supervisor = false) : operation(operation), is_supervisor(is_supervisor), opcode(opcode) {}

	// Instruction fields are decoded below; naming is as directly dictated by
	// Motorola's documentation, and the definitions below are sorted by synonym.
	uint16_t uimm() {	return uint16_t(opcode & 0xffff);	}
	int16_t simm()	{	return int16_t(opcode & 0xffff);	}

	int to() 		{	return (opcode >> 21) & 0x1f;		}
	int d() 		{	return (opcode >> 21) & 0x1f;		}
	int bo() 		{	return (opcode >> 21) & 0x1f;		}
	int crbD() 		{	return (opcode >> 21) & 0x1f;		}
	int s() 		{	return (opcode >> 21) & 0x1f;		}

	int a() 		{	return (opcode >> 16) & 0x1f;		}
	int bi() 		{	return (opcode >> 16) & 0x1f;		}
	int crbA() 		{	return (opcode >> 16) & 0x1f;		}

	int b() 		{	return (opcode >> 11) & 0x1f;		}
	int crbB() 		{	return (opcode >> 11) & 0x1f;		}

	int c() 		{	return (opcode >> 6) & 0x1f;		}

	int crfd() 		{	return (opcode >> 23) & 0x07;		}
	
	int bd()		{	return (opcode >> 2) & 0x3fff;		}
	
	int li()		{	return (opcode >> 2) & 0x0fff;		}

	// Various single bit fields.
	int l() 		{	return (opcode >> 21) & 0x01;		}
	int aa()		{	return (opcode >> 1) & 0x01;		}
	int lk()		{	return opcode & 0x01;				}
	int rc()		{	return opcode & 0x01;				}
};

struct Decoder {
	public:
		Decoder(Model model);

		Instruction decode(uint32_t opcode);

	private:
		Model model_;

		bool is64bit() {
			return false;
		}

		bool is32bit() {
			return true;
		}

		bool is601() {
			return model_ == Model::MPC601;
		}
};

}
}
}

#include <stdio.h>

#endif /* PowerPC_hpp */
