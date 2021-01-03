//
//  PowerPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/30/20.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef PowerPC_hpp
#define PowerPC_hpp

#include <cstddef>
#include <cstdint>

namespace CPU {
namespace Decoder {
namespace PowerPC {

enum class Model {
	/// i.e. 32-bit, with POWER carry-over instructions.
	MPC601,
	/// i.e. 32-bit, no POWER instructions.
	MPC603,
	/// i.e. 64-bit.
	MPC620,
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
	fresx, frsqrtex, fselx, fsqrtx, slbia, slbie, stfiwx,

	// 64-bit only PowerPC instructions.
	cntlzdx, divdx, divdux, extswx, fcfidx, fctidx, fctidzx, tdi, mulhdux,
	ldx, sldx, ldux, td, mulhdx, ldarx, stdx, stdux, mulld, lwax, lwaux,
	sradix, srdx, sradx, extsw, fsqrtsx, std, stdu, stdcx_,
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

	// PowerPC uses a fixed-size instruction word.
	size_t size() {
		return 4;
	}

	Instruction(uint32_t opcode) : opcode(opcode) {}
	Instruction(Operation operation, uint32_t opcode, bool is_supervisor = false) : operation(operation), is_supervisor(is_supervisor), opcode(opcode) {}

	// Instruction fields are decoded below; naming is a compromise between
	// Motorola's documentation and IBM's.
	//
	// I've dutifully implemented various synonyms with unique entry points,
	// in order to capture that information here rather than thrusting it upon
	// the reader of whatever implementation may follow.

	// Currently omitted: OPCD and XO, which I think are unnecessary given that
	// full decoding has already occurred.

	/// Immediate field used to specify an unsigned 16-bit integer.
	uint16_t uimm() {	return uint16_t(opcode & 0xffff);	}
	/// Immediate field used to specify a signed 16-bit integer.
	int16_t simm()	{	return int16_t(opcode & 0xffff);	}
	/// Immediate field used to specify a signed 16-bit integer.
	int16_t d()		{	return int16_t(opcode & 0xffff);	}
	/// Immediate field used to specify a signed 14-bit integer [64-bit only].
	int16_t ds()	{	return int16_t(opcode & 0xfffc);	}
	/// Immediate field used as data to be placed into a field in the floating point status and condition register.
	int32_t imm()	{	return (opcode >> 12) & 0xf;		}

	/// Specifies the conditions on which to trap.
	int32_t to() 	{	return (opcode >> 21) & 0x1f;		}

	/// Register source A or destination.
	uint32_t rA() 	{	return (opcode >> 16) & 0x1f;		}
	/// Register source B.
	uint32_t rB() 	{	return (opcode >> 11) & 0x1f;		}
	/// Register destination.
	uint32_t rD() 	{	return (opcode >> 21) & 0x1f;		}
	/// Register source.
	uint32_t rS() 	{	return (opcode >> 21) & 0x1f;		}

	/// Floating point register source A.
	uint32_t frA() 	{	return (opcode >> 16) & 0x1f;		}
	/// Floating point register source B.
	uint32_t frB() 	{	return (opcode >> 11) & 0x1f;		}
	/// Floating point register source C.
	uint32_t frC() 	{	return (opcode >> 6) & 0x1f;		}
	/// Floating point register source.
	uint32_t frS() 	{	return (opcode >> 21) & 0x1f;		}
	/// Floating point register destination.
	uint32_t frD() 	{	return (opcode >> 21) & 0x1f;		}

	/// Branch conditional options.
	uint32_t bo() 	{	return (opcode >> 21) & 0x1f;		}
	/// Source condition register bit for branch conditionals.
	uint32_t bi() 	{	return (opcode >> 16) & 0x1f;		}
	/// Branch displacement; provided as already sign extended.
	int16_t bd()	{	return int16_t(opcode & 0xfffc);	}

	/// Specifies the first 1 bit of a 32/64-bit mask for rotate operations.
	uint32_t mb()	{	return (opcode >> 6) & 0x1f;		}
	/// Specifies the first 1 bit of a 32/64-bit mask for rotate operations.
	uint32_t me()	{	return (opcode >> 1) & 0x1f;		}

	/// Condition register source bit A.
	uint32_t crbA() {	return (opcode >> 16) & 0x1f;		}
	/// Condition register source bit B.
	uint32_t crbB() {	return (opcode >> 11) & 0x1f;		}
	/// Condition register (or floating point status & condition register) destination bit.
	uint32_t crbD() {	return (opcode >> 21) & 0x1f;		}

	/// Condition register (or floating point status & condition register) destination field.
	uint32_t crfD() {	return (opcode >> 23) & 0x07;		}
	/// Condition register (or floating point status & condition register) source field.
	uint32_t crfS() {	return (opcode >> 18) & 0x07;		}

	/// Mask identifying fields to be updated by mtcrf.
	uint32_t crm()	{	return (opcode >> 12) & 0xff;		}

	/// Mask identifying fields to be updated by mtfsf.
	uint32_t fm()	{	return (opcode >> 17) & 0xff;		}

	/// Specifies the number of bytes to move in an immediate string load or store.
	uint32_t nb()	{	return (opcode >> 11) & 0x1f;		}

	/// Specifies a shift amount.
	/// TODO: possibly bit 30 is also used in 64-bit mode, find out.
	uint32_t sh()	{	return (opcode >> 11) & 0x1f;		}

	/// Specifies one of the 16 segment registers [32-bit only].
	uint32_t sr()	{	return (opcode >> 16) & 0xf;		}

	/// A 24-bit signed number; provided as already sign extended.
	int32_t li() {
		constexpr uint32_t extensions[2] = {
			0x0000'0000,
			0xfc00'0000
		};
		const uint32_t value = (opcode & 0x3fff'fffc) | extensions[(opcode >> 26) & 1];
		return int32_t(value);
	}

	/// Absolute address bit; @c 0 or @c non-0.
	uint32_t aa()	{	return opcode & 0x02;		}
	/// Link bit; @c 0 or @c non-0.
	uint32_t lk()	{	return opcode & 0x01;		}
	/// Record bit; @c 0 or @c non-0.
	uint32_t rc()	{	return opcode & 0x01;		}
	/// Whether to compare 32-bit or 64-bit numbers [for 64-bit implementations only]; @c 0 or @c non-0.
	uint32_t l() 	{	return opcode & 0x200000;	}
	/// Enables setting of OV and SO in the XER; @c 0 or @c non-0.
	uint32_t oe()	{	return opcode & 0x800;		}
};

/*!
	Implements PowerPC instruction decoding.

	This is an experimental implementation; it has not yet undergone significant testing.
*/
struct Decoder {
	public:
		Decoder(Model model);

		Instruction decode(uint32_t opcode);

	private:
		Model model_;

		bool is64bit() {
			return model_ == Model::MPC620;
		}

		bool is32bit() {
			return !is64bit();
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
