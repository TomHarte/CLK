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

// TODO: complete the following table.
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
	subfex, subfic, subfmex, subfzex, sync, tlbie, tw, twi, xorx, xori, xoris,
	
	// 32-bit, supervisor level.
	dcbi,
	
	// Optional.
	fresx, frsqrtex, fselx, fsqrtx, frsqrtsx, slbia, slbie,

	// 64-bit only PowerPC instructions.
	cntlzdx, divdx, divdux, extswx, fcfidx, fctidx, fctidzx
};

struct Instruction {
	Operation operation = Operation::Undefined;

	//

	Instruction() {}
	Instruction(Operation operation) : operation(operation) {}
};

struct Decoder {
	public:
		Decoder(Model model);

		Instruction decode(uint32_t opcode);

	private:
		Model model_;
};

}
}
}

#include <stdio.h>

#endif /* PowerPC_hpp */
