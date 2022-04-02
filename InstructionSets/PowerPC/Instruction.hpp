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
	//	condition ending Set or Clear => test the condition bit.
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

	//
	// MARK: - 601-exclusive instructions.
	//
	// A lot of them are carry-overs from POWER, left in place
	// due to the tight original development timeline.
	//
	// These are not part of the PowerPC architecture.

	/// Absolute.
	/// abs abs. abso abso.
	/// rD(), rA()	[oe(), rc()]
	absx,

	/// Cache line compute size.
	/// clcs
	/// rD(), rA()
	clcs,

	/// Divide.
	/// div div. divo divo.
	/// rD(), rA(), rB()	[rc(), oe()]
	divx,

	/// Divide short.
	/// divs divs. divso divso.
	/// rD(), rA(), rB()	[rc(), eo()]
	divsx,

	/// Difference or zero.
	/// doz doz. dozo dozo.
	/// rD(), rA(), rB()	[rc(), oe()]
	dozx,

	/// Difference or zero immediate.
	/// dozi
	/// rD(), rA(), simm()
	dozi,

	lscbxx, maskgx, maskirx,

	/// Multiply.
	/// mul mul. mulo mulo.
	/// rA(), rB(), rD()
	mulx,

	nabsx, rlmix, rribx, slex, sleqx, sliqx, slliqx, sllqx, slqx,
	sraiqx, sraqx, srex, sreax, sreqx, sriqx, srliqx, srlqx, srqx,

	//
	// MARK: - 32- and 64-bit PowerPC instructions.
	//

	/// Add.
	/// add add. addo addo.
	/// rD(), rA(), rB()	[rc(), oe()]
	addx,

	/// Add carrying.
	/// addc addc. addco addco.
	/// rD(), rA(), rB()	[rc(), oe()]
	addcx,

	/// Add extended.
	/// adde adde. addeo addeo.
	/// rD(), rA(), rB()	[rc(), eo()]
	addex,

	/// Add immediate.
	/// addi
	/// rD(), rA(), simm()
	addi,

	/// Add immediate carrying.
	/// addic
	/// rD(), rA(), simm()
	addic,

	/// Add immediate carrying and record.
	/// addic.
	/// rD(), rA(), simm()
	addic_,

	/// Add immediate shifted.
	/// addis.
	/// rD(), rA(), simm()
	addis,

	/// Add to minus one.
	/// addme addme. addmeo addmeo.
	/// rD(), rA()	[rc(), oe()]
	addmex,

	/// Add to zero extended.
	/// addze addze. addzeo addzeo.
	/// rD(), rA()	[rc(), oe()]
	addzex,

	/// And.
	/// and, and.
	/// rA(), rS(), rB()	[rc()]
	andx,

	/// And with complement.
	/// andc, andc.
	/// rA(), rS(), rB()	[rc()]
	andcx,

	/// And immediate.
	/// andi.
	/// rA(), rS(), uimm()
	andi_,

	/// And immediate shifted.
	/// andis.
	/// rA(), rS(), uimm()
	andis_,

	/// Branch unconditional.
	/// b bl ba bla
	/// li()	[aa(), lk()]
	bx,

	/// Branch conditional.
	/// bne bne+ beq bdnzt+ bdnzf bdnzt bdnzfla ...
	/// bo(), bi(), bd()	[aa(), lk()]
	bcx,

	/// Branch conditional to count register.
	/// bctr bctrl bnectrl bnectrl bltctr blectr ...
	/// bo(), bi()	[aa(), lk()]
	bcctrx,

	/// Branch conditional to link register.
	/// blr blrl bltlr blelrl bnelrl ...
	/// bo(), bi()	[aa(), lk()]
	bclrx,

	/// Compare
	/// cmp
	/// crfD(), l(), rA(), rB()
	cmp,

	/// Compare immediate.
	/// cmpi
	/// crfD(), l(), rA(), simm()
	cmpi,

	/// Compare logical.
	/// cmpl
	/// crfD(), l(), rA(), rB()
	cmpl,

	/// Compare logical immediate.
	/// cmpli
	/// crfD(), l(), rA(), uimm()
	cmpli,

	/// Count leading zero words.
	/// cntlzw cntlzw.
	/// rA(), rS()	[rc()]
	cntlzwx,

	/// Condition register and.
	/// crand
	/// crbD(), crbA(), crbB()
	crand,

	/// Condition register and with complement.
	/// crandc
	/// crbD(), crbA(), crbB()
	crandc,

	/// Condition register equivalent.
	/// creqv
	/// crbD(), crbA(), crbB()
	creqv,

	/// Condition register nand.
	/// crnand
	/// crbD(), crbA(), crbB()
	crnand,

	/// Condition register nor.
	/// crnor
	/// crbD(), crbA(), crbB()
	crnor,

	/// Condition register or.
	/// cror
	/// crbD(), crbA(), crbB()
	cror,

	/// Condition register or with complement.
	/// crorc
	/// crbD(), crbA(), crbB()
	crorc,

	/// Condition register xor.
	/// crxor
	/// crbD(), crbA(), crbB()
	crxor,

	/// Data cache block flush.
	/// dcbf
	/// rA(), rB()
	dcbf,

	/// Data cache block store.
	/// dcbst
	/// rA(), rB()
	dcbst,

	/// Data cache block touch.
	/// dcbt
	/// rA(), rB()
	dcbt,

	/// Data cache block touch for store.
	/// dcbtst
	/// rA(), rB()
	dcbtst,

	/// Data cache block set to zero.
	/// dcbz
	/// rA(), rB()
	dcbz,

	/// Divide word.
	/// divw divw. divwo divwo.
	/// rD(), rA(), rB()	[rc(), oe()]
	divwx,

	/// Divide word unsigned.
	/// divwu divwu. divwuo divwuo.
	/// rD(), rA(), rB()	[rc(), oe()]
	divwux,

	/// External control in word indexed.
	/// eciwx
	/// rD(), rA(), rB()
	eciwx,

	/// External control out word indexed.
	/// ecowx
	/// rS(), rA(), rB()
	ecowx,

	/// Enforce in-order execition of I/O
	/// eieio
	eieio,

	/// Equivalent.
	/// eqv eqv.
	/// rA(), rS(), rB()	[rc()]
	eqvx,

	/// Extend sign byte.
	/// extsb extsb.
	/// rA(), rS()	[rc()]
	extsbx,

	/// Extend sign half-word.
	/// extsh extsh.
	/// rA(), rS()	[rc()]
	extshx,

	fabsx, faddx, faddsx, fcmpo, fcmpu, fctiwx, fctiwzx,
	fdivx, fdivsx, fmaddx, fmaddsx, fmrx, fmsubx, fmsubsx, fmulx, fmulsx,
	fnabsx, fnegx, fnmaddx, fnmaddsx, fnmsubx, fnmsubsx, frspx, fsubx, fsubsx,
	icbi, isync, lbz, lbzu,

	/// Load byte and zero with update indexed.
	/// lbzux
	lbzux,

	/// Load byte and zero indexed.
	/// lbzx
	lbzx,

	lfd, lfdu, lfdux, lfdx, lfs, lfsu,
	lfsux, lfsx, lha, lhau,

	/// Load half-word algebraic with update indexed.
	/// lhaux
	/// rD(), rA(), rB()
	lhaux,

	/// Load half-word algebraic indexed.
	/// lhax
	/// rD(), rA(), rB()
	lhax,

	/// Load half word byte-reverse indexed.
	/// lhbrx
	/// rD(), rA(), rB()
	lhbrx,

	/// Load half word and zero.
	/// lhz
	/// rD(), d()[ rA() ]
	lhz,

	/// Load half-word and zero with update.
	/// lhzu
	/// rD(), d()[ rA() ]
	lhzu,

	/// Load half-word and zero with update indexed.
	/// lhzux
	/// rD(), rA(), rB()
	lhzux,

	/// Load half-word and zero indexed.
	/// lhzx
	/// rD(), rA(), rB()
	lhzx,

	lmw,
	lswi, lswx, lwarx, lwbrx, lwz, lwzu,

	/// Load word and zero with update indexed.
	/// lwzux
	lwzux,

	/// Load word and zero indexed.
	/// lwzx
	lwzx,

	mcrf, mcrfs, mcrxr,
	mfcr, mffsx, mfmsr, mfspr, mfsr, mfsrin,

	/// Move to condition register fields.
	/// mtcrf
	/// rS(), crm()
	mtcrf,

	mtfsb0x, mtfsb1x, mtfsfx,
	mtfsfix, mtmsr, mtspr, mtsr, mtsrin,

	/// Multiply high word.
	/// mulhw mulgw.
	/// rD(), rA(), rB(), rc()
	mulhwx,

	/// Multiply high word unsigned.
	/// mulhwu mulhwu.
	/// rD(), rA(), rB(), rc()
	mulhwux,

	/// Multiply low immediate.
	mulli,

	/// Multiply low word.
	/// mullw mullw. mullwo mullwo.
	/// rA(), rB(), rD()
	mullwx,

	nandx, negx, norx, orx, orcx, ori, oris, rfi, rlwimix,

	/// Rotate left word immediate then AND with mask.
	/// rlwinm rlwinm.
	/// rA(), rS(), sh(), mb(), me(), rc()
	rlwinmx,

	/// Rotate left word then AND with mask
	/// rlwimi rlwimi.
	/// rA(), rB(), rS(), mb(), me(), rc()
	rlwnmx,

	/// System call.
	/// sc
	sc,

	/// Shift left word.
	/// slw slw.
	/// rA(), rS(), rB()	[rc()]
	slwx,

	/// Shift right algebraic word.
	/// sraw sraw.
	/// rA(), rS(), rB()	[rc()]
	srawx,

	/// Shift right algebraic word immediate.
	/// srawi srawi.
	/// rA(), rS(), sh()	[rc()]
	srawix,

	/// Shift right word.
	/// srw srw.
	/// rA(), rS(), rB()	[rc()]
	srwx,

	/// Store byte.
	/// stb
	/// rS(), d() [ rA() ]
	stb,

	/// Store byte with update.
	/// stbu
	/// rS(), d() [ rA() ]
	stbu,

	/// Store byte with update indexed.
	/// stbux
	/// rS(), rA(), rB()
	stbux,

	/// Store byte indexed.
	/// stbx
	/// rS(), rA(), rB()
	stbx,

	/// Store floating point double precision.
	/// stfd
	/// frS(), d() [ rA() ]
	stfd,

	/// Store floating point double precision with update.
	/// stfdu
	/// frS(), d() [ rA() ]
	stfdu,

	/// Store floating point double precision with update indexed.
	/// stfdux
	/// frS(), rA(), rB()
	stfdux,

	/// Store floating point double precision indexed.
	/// stfdux
	/// frS(), rA(), rB()
	stfdx,

	/// Store floating point single precision.
	/// stfs
	/// frS() d() [ rA() ]
	stfs,

	/// Store floating point single precision with update.
	/// stfs
	/// frS() d() [ rA() ]
	stfsu,

	stfsux, stfsx, sth, sthbrx, sthu,

	/// Store half-word with update indexed.
	sthux,

	/// Store half-word indexed.
	sthx,

	stmw, stswi, stswx, stw,

	/// Store word byte-reverse indexed.
	/// stwbrx
	/// rS(), rA(), rB()
	stwbrx,

	/// Store word conditional.
	/// stwcx.
	/// rS(), rA(), rB()
	stwcx_,

	/// Store word with update.
	/// stwu
	/// rS(), d() [ rA() ]
	stwu,

	/// Store word with update indexed.
	/// stwux
	/// rS(), rA(), rB()
	stwux,

	/// Store word indexed.
	/// stwx
	/// rS(), rA(), rB()
	stwx,

	subfx,

	/// Subtract from carrying.
	/// subfc subfc. subfco subfco.
	subfcx,

	subfex,

	/// Subtract from immediate carrying
	subfic,

	subfmex, subfzex, sync,

	/// Trap word.
	/// tw tweq tweqi twge twgei ...
	/// to(), rA(), rB()
	tw,

	/// Trap word immediate.
	/// twi
	/// to(), rA(), simm()
	twi,

	/// Xor.
	/// xor xor.
	/// rA(), rS(), rB()	[rc()]
	xorx,

	/// Xor immediate.
	/// xori
	/// rA(), rs(), uimm()
	xori,

	/// Xor immediate shifted.
	/// xoris
	/// rA(), rS(), uimm()
	xoris,

	//
	// MARK: - 32-bit, supervisor level.
	//

	/// Data cache block invalidate.
	/// dcbi
	/// rA(), rB()
	dcbi,

	//
	// MARK: - Supervisor, optional.
	//
	tlbia, tlbie, tlbsync,

	//
	// MARK: - Optional.
	//
	fresx, frsqrtex, fselx, fsqrtx,

	/// Move from time base.
	/// mftb
	/// rD(), tbr()
	mftb,


	slbia, slbie,

	/// Store floating point as integer word indexed.
	/// stfiwx
	/// frS(), rA(), rB()
	stfiwx,

	//
	// MARK: - 64-bit only PowerPC instructions.
	//
	cntlzdx,

	/// Divide double word.
	/// divd divd. divdo divdo.
	/// rD(), rA(), rB()	[rc(), oe()]
	divdx,

	/// Divide double word unsigned.
	/// divdu divdu. divduo divduo.
	/// rD(), rA(), rB()	[rc(), oe()]
	divdux,

	/// Extend sign word.
	/// extsw extsw.
	/// rA(), rS()	[rc()]
	extswx,

	fcfidx, fctidx, fctidzx, tdi, mulhdux,
	ldx, sldx, ldux, td, mulhdx, ldarx,

	/// Store double.
	/// std
	/// rS(), ds() [ rA() ]
	std,

	/// Store double word conditional indexed.
	/// stdcx.
	/// rS(), rA(), rB()
	stdcx_,

	/// Store double word with update.
	/// stdu
	/// rS(), ds() [ rA() ]
	stdu,

	/// Store double word with update indexed.
	/// stdux
	/// rS(), rA(), rB()
	stdux,

	/// Store double word indexed.
	/// stdx
	/// rS(), rA(), rB()
	stdx,

	mulld, lwax, lwaux,
	sradix, srdx,

	/// Shift right algebraic double word.
	/// srad srad,
	/// rA(), rS(), rB()	[rc()]
	sradx,

	fsqrtsx
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
	uint32_t oe() const	{	return opcode & 0x400;		}
};

// Sanity check on Instruction size.
static_assert(sizeof(Instruction) <= 8);

}
}

#endif /* InstructionSets_PowerPC_Instruction_h */
