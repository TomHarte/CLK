//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_PowerPC_Instruction_h
#define InstructionSets_PowerPC_Instruction_h

#include <cstdint>

namespace InstructionSet {
namespace PowerPC {

enum class CacheLine: uint32_t {
	Instruction = 0b01100,
	Data = 0b1101,
	Minimum = 0b01110,
	Maximum = 0b01111,
};

enum class Condition: uint32_t {
	// CR0
	Negative = 0,				// LT
	Positive = 1,				// GT
	Zero = 2,					// EQ
	SummaryOverflow = 3,		// SO

	// CR1
	FPException = 4,			// FX
	FPEnabledException = 5,		// FEX
	FPInvalidException = 6,		// VX
	FPOverflowException = 7,	// OX

	// CRs2–7 fill out the condition register.
};

enum class BranchOption: uint32_t {
	// Naming convention:
	//
	//	Dec_ prefix => decrement the CTR;
	//	condition starting NotZero or Zero => test CTR;
	//	condition ending Set or Clear => test for condition bit.
	//
	// Numerical suffixes are present because there's some redundancy
	// in encodings.
	//
	// Note that the encodings themselves may suggest alternative means
	// of interpretation than mapping via this enum.
	Dec_NotZeroAndClear	= 0b0000,
	Dec_ZeroAndClear	= 0b0001,
	Clear 				= 0b0010,
	Dec_NotZeroAndSet	= 0b0100,
	Dec_ZeroAndSet 		= 0b0101,
	Set 				= 0b0110,
	Dec_NotZero 		= 0b1000,
	Dec_Zero			= 0b1001,
	Always 				= 0b1010,
};

enum class Operation: uint8_t {
	Undefined,

	// These 601-exclusive instructions; a lot of them are carry-overs
	// from POWER. These are not part of the PowerPC architecture.

	/// |rA| is placed into rD. If rA = 0x8000'0000 then 0x8000'0000 is placed into rD
	/// and XER[OV] is set if oe() indicates that overflow is enabled.
	absx,

	/// The size of the cache line specified by rA is placed into rD. Cf. the CacheLine enum.
	/// As an aside: all cache lines are 64 bytes on the MPC601.
	clcs,

	/// div, div., divo, div.; unsigned 64-bit divide. rA|MQ / rB is placed into rD and the
	/// remainder is placed into MQ. The ermainder has the same sign as the dividend
	/// such that remainder + divisor * quotient = dividend.
	///
	/// rc() != 0 => the LT, GT and EQ bits in CR are set as per the remainder.
	/// oe() != 0 => SO and OV are set if the quotient exceeds 32 bits.
	divx,

	/// divs, divs., divso, divso.; signed 32-bit divide. rD = rA/rB; remainder is
	/// placed into MQ. The ermainder has the same sign as the dividend
	/// such that remainder + divisor * quotient = dividend.
	///
	/// rc() != 0 => the LT, GT and EQ bits in CR are set as per the remainder.
	/// oe() != 0 => SO and OV are set if the quotient exceeds 32 bits.
	divsx,

	/// if rA > rB then rD = 0; else rD = NOT(rA) + rB + 1.
	dozx,

	/// if rA > simm() then rD = 0; else rD = NOT(rA) + simm() + 1.
	dozi,

	lscbxx, maskgx, maskirx, mulx,
	nabsx, rlmix, rribx, slex, sleqx, sliqx, slliqx, sllqx, slqx,
	sraiqx, sraqx, srex, sreax, sreqx, sriqx, srliqx, srlqx, srqx,

	// 32- and 64-bit PowerPC instructions.
	addx, addcx, addex, addi, addic, addic_, addis, addmex, addzex, andx,
	andcx, andi_, andis_,

	/// Branch unconditional.
	///
	/// Use li() to get the included immediate value.
	///
	/// Use aa() to determine whether it's a relative (aa() = 0) or absolute (aa() != 0) address.
	/// Also check lk() to determine whether to update the link register.
	///
	/// Synonyms include:
	/// 	* b (relative, no link) [though assemblers might encode as a bcx];
	/// 	* bl (relative, link);
	/// 	* ba (absolute, no link);
	/// 	* bla (absolute, link).
	bx,

	/// Branch conditional.
	///
	/// aa() determines whether the branch has a relative or absolute target.
	/// lk() determines whether to update the link register.
	/// bd() supplies a relative displacment or absolute address.
	/// bi() specifies which CR bit to use as a condition; cf. the Condition enum.
	/// bo() provides other branch options and a branch prediction hint as per (BranchOptions enum << 1) | hint.
	///
	/// Synonyms incude:
	/// 	* b (relative, no link) [though assemblers might encode as a bx].
	bcx,

	/// Branch conditional to count register.
	///
	/// aa(), bi(), bo() and lk() are as per bcx.
	///
	/// On the MPC601, anything that decrements the count register will use the non-decremented
	/// version as the branch target. Other processors will use the decremented version.
	bcctrx,

	/// Branch conditional to link register.
	///
	/// aa(), bi(), bo() and lk() are as per bcx.
	bclrx,

	cmp, cmpi, cmpl, cmpli,
	cntlzwx, crand, crandc, creqv, crnand, crnor, cror, crorc, crxor, dcbf,
	dcbst, dcbt, dcbtst, dcbz, divwx, divwux, eciwx, ecowx, eieio, eqvx,
	extsbx, extshx, fabsx, faddx, faddsx, fcmpo, fcmpu, fctiwx, fctiwzx,
	fdivx, fdivsx, fmaddx, fmaddsx, fmrx, fmsubx, fmsubsx, fmulx, fmulsx,
	fnabsx, fnegx, fnmaddx, fnmaddsx, fnmsubx, fnmsubsx, frspx, fsubx, fsubsx,
	icbi, isync, lbz, lbzu, lbzux, lbzx, lfd, lfdu, lfdux, lfdx, lfs, lfsu,
	lfsux, lfsx, lha, lhau, lhaux, lhax, lhbrx, lhz, lhzu, lhzux, lhzx, lmw,
	lswi, lswx, lwarx, lwbrx, lwz, lwzu, lwzux,

	/// Load word and zero indexed.
	///
	/// rD() = [ (rA()|0) + rB() ] i.e. if rA() is 0 then the value 0 is used, not the contents of r0.
	lwzx,

	mcrf, mcrfs, mcrxr,
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

	It would be possible to partition the ordering of Operations into user followed by supervisor,
	eliminating the storage necessary for a flag, but it wouldn't save anything due to alignment.
*/
struct Instruction {
	Operation operation = Operation::Undefined;
	bool is_supervisor = false;
	uint32_t opcode = 0;

	Instruction() noexcept {}
	Instruction(uint32_t opcode) noexcept : opcode(opcode) {}
	Instruction(Operation operation, uint32_t opcode, bool is_supervisor = false) noexcept : operation(operation), is_supervisor(is_supervisor), opcode(opcode) {}

	// Instruction fields are decoded below; naming is a compromise between
	// Motorola's documentation and IBM's.
	//
	// I've dutifully implemented various synonyms with unique entry points,
	// in order to capture that information here rather than thrusting it upon
	// the reader of whatever implementation may follow.

	// Currently omitted: OPCD and XO, which I think are unnecessary given that
	// full decoding has already occurred.

	/// Immediate field used to specify an unsigned 16-bit integer.
	uint16_t uimm() const	{	return uint16_t(opcode & 0xffff);	}
	/// Immediate field used to specify a signed 16-bit integer.
	int16_t simm() const	{	return int16_t(opcode & 0xffff);	}
	/// Immediate field used to specify a signed 16-bit integer.
	int16_t d() const		{	return int16_t(opcode & 0xffff);	}
	/// Immediate field used to specify a signed 14-bit integer [64-bit only].
	int16_t ds() const		{	return int16_t(opcode & 0xfffc);	}
	/// Immediate field used as data to be placed into a field in the floating point status and condition register.
	int32_t imm() const		{	return (opcode >> 12) & 0xf;		}

	/// Specifies the conditions on which to trap.
	int32_t to() const	 	{	return (opcode >> 21) & 0x1f;		}

	/// Register source A or destination.
	uint32_t rA() const 	{	return (opcode >> 16) & 0x1f;		}
	/// Register source B.
	uint32_t rB() const 	{	return (opcode >> 11) & 0x1f;		}
	/// Register destination.
	uint32_t rD() const 	{	return (opcode >> 21) & 0x1f;		}
	/// Register source.
	uint32_t rS() const 	{	return (opcode >> 21) & 0x1f;		}

	/// Floating point register source A.
	uint32_t frA() const 	{	return (opcode >> 16) & 0x1f;		}
	/// Floating point register source B.
	uint32_t frB() const 	{	return (opcode >> 11) & 0x1f;		}
	/// Floating point register source C.
	uint32_t frC() const 	{	return (opcode >> 6) & 0x1f;		}
	/// Floating point register source.
	uint32_t frS() const 	{	return (opcode >> 21) & 0x1f;		}
	/// Floating point register destination.
	uint32_t frD() const 	{	return (opcode >> 21) & 0x1f;		}

	/// Branch conditional options as per PowerPC spec, i.e. options + branch-prediction flag.
	uint32_t bo() const 	{	return (opcode >> 21) & 0x1f;		}
	/// Just the branch options, with the branch prediction flag severed.
	BranchOption branch_options() const {
		return BranchOption((opcode >> 22) & 0xf);
	}
	/// Just the branch-prediction hint; @c 0 => expect untaken; @c non-0 => expect take.
	uint32_t branch_prediction_hint() const {
		return opcode & 0x200000;
	}
	/// Source condition register bit for branch conditionals.
	uint32_t bi() const 	{	return (opcode >> 16) & 0x1f;		}
	/// Branch displacement; provided as already sign extended.
	int16_t bd() const		{	return int16_t(opcode & 0xfffc);	}

	/// Specifies the first 1 bit of a 32/64-bit mask for rotate operations.
	uint32_t mb() const		{	return (opcode >> 6) & 0x1f;		}
	/// Specifies the first 1 bit of a 32/64-bit mask for rotate operations.
	uint32_t me() const		{	return (opcode >> 1) & 0x1f;		}

	/// Condition register source bit A.
	uint32_t crbA() const	{	return (opcode >> 16) & 0x1f;		}
	/// Condition register source bit B.
	uint32_t crbB() const	{	return (opcode >> 11) & 0x1f;		}
	/// Condition register (or floating point status & condition register) destination bit.
	uint32_t crbD() const	{	return (opcode >> 21) & 0x1f;		}

	/// Condition register (or floating point status & condition register) destination field.
	uint32_t crfD() const	{	return (opcode >> 23) & 0x07;		}
	/// Condition register (or floating point status & condition register) source field.
	uint32_t crfS() const	{	return (opcode >> 18) & 0x07;		}

	/// Mask identifying fields to be updated by mtcrf.
	uint32_t crm() const	{	return (opcode >> 12) & 0xff;		}

	/// Mask identifying fields to be updated by mtfsf.
	uint32_t fm() const		{	return (opcode >> 17) & 0xff;		}

	/// Specifies the number of bytes to move in an immediate string load or store.
	uint32_t nb() const		{	return (opcode >> 11) & 0x1f;		}

	/// Specifies a shift amount.
	/// TODO: possibly bit 30 is also used in 64-bit mode, find out.
	uint32_t sh() const		{	return (opcode >> 11) & 0x1f;		}

	/// Specifies one of the 16 segment registers [32-bit only].
	uint32_t sr() const		{	return (opcode >> 16) & 0xf;		}

	/// A 24-bit signed number; provided as already sign extended.
	int32_t li() const {
		constexpr uint32_t extensions[2] = {
			0x0000'0000,
			0xfc00'0000
		};
		const uint32_t value = (opcode & 0x03ff'fffc) | extensions[(opcode >> 25) & 1];
		return int32_t(value);
	}

	/// Absolute address bit; @c 0 or @c non-0.
	uint32_t aa() const	{	return opcode & 0x02;		}
	/// Link bit; @c 0 or @c non-0.
	uint32_t lk() const	{	return opcode & 0x01;		}
	/// Record bit; @c 0 or @c non-0.
	uint32_t rc() const	{	return opcode & 0x01;		}
	/// Whether to compare 32-bit or 64-bit numbers [for 64-bit implementations only]; @c 0 or @c non-0.
	uint32_t l() const	{	return opcode & 0x200000;	}
	/// Enables setting of OV and SO in the XER; @c 0 or @c non-0.
	uint32_t oe() const	{	return opcode & 0x800;		}
};

// Sanity check on Instruction size.
static_assert(sizeof(Instruction) <= 8);

}
}

#endif /* InstructionSets_PowerPC_Instruction_h */
