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

/// Provides the meaning of individual bits within the condition register;
/// bits are counted in IBM/Motorola order, so *bit 0 is the most significant.*
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


/// @returns @c 0 if reg == 0; @c ~0 otherwise.
/// @discussion Provides a branchless way to substitute the value 0 for the value of r0
/// in affected instructions. Assumes arithmetic shifts.
template <typename IntT> constexpr IntT is_zero_mask(uint32_t reg) {
	return ~IntT((int(reg) - 1) >> 5);
}

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

	/// Divide short.
	/// divs divs. divso divso.
	/// rD(), rA(), rB()	[rc(), eo()]
	divsx,

	/// Divide.
	/// div div. divo divo.
	/// rD(), rA(), rB()	[rc(), oe()]
	divx,

	/// Difference or zero immediate.
	/// dozi
	/// rD(), rA(), simm()
	dozi,

	/// Difference or zero.
	/// doz doz. dozo dozo.
	/// rD(), rA(), rB()	[rc(), oe()]
	dozx,

	/// Load string and compare byte indexed.
	/// lscbx lsxbx.
	/// rD(), rA(), rB()	[rc()]
	lscbxx,

	/// Mask generate.
	/// maskg maskg.
	/// rA(), rS(), rB()	[rc()]
	maskgx,

	/// Mask insert from register.
	/// maskir maskir.
	/// rA(), rS(), rB()	[rc()]
	maskirx,

	/// Multiply.
	/// mul mul. mulo mulo.
	/// rA(), rB(), rD()
	mulx,

	/// Negative absolute.
	/// nabs nabs. nabso nabso.
	/// rD(), rA()	[rc(), oe()]
	nabsx,

	/// Rotate left then mask insert.
	/// rlmi rlmi.
	/// rA(), rS(), rB(), mb(), me()	[rc()]
	/// Cf. rotate_mask()
	rlmix,

	/// Rotate right and insert bit.
	/// rrib rrib.
	/// rA(), rS(), rB()	[rc()]
	rribx,

	/// Shift left extended with MQ.
	/// sleq sleq.
	/// rA(), rS(), rB()	[rc()]
	sleqx,

	/// Shift left extended.
	/// sle sle.
	/// rA(), rS(), rB()	[rc()]
	slex,

	/// Shift left immediate with MQ.
	/// sliq sliq.
	/// rA(), rS(), sh()	[rc()]
	sliqx,

	/// Shift left long immediate with MQ.
	/// slliq slliq.
	/// rA(), rS(), sh()	[rc()]
	slliqx,

	/// Shift left long with MQ.
	/// sllq sllq.
	/// rA(), rS(), rB()	[rc()]
	sllqx,

	/// Shift left with MQ.
	/// slq slq.
	/// rA(), rS(), rB()	[rc()]
	slqx,

	/// Shift right algebraic immediate with MQ.
	/// sraiq sraiq.
	/// rA(), rS(), sh()	[rc()]
	sraiqx,

	/// Shift right algebraic with MQ.
	/// sraq sraq.
	/// rA(), rS(), rB()	[rc()]
	sraqx,

	/// Shift right extended algebraic.
	/// srea srea.
	/// rA(), rS(), rB()	[rc()]
	sreax,

	/// Shift right extended with MQ.
	/// sreq sreq.
	/// rA(), rS(), rB()	[rc()]
	sreqx,

	/// Shift right extended.
	/// sre sre.
	/// rA(), rS(), rB()	[rc()]
	srex,

	/// Shift right immediate with MQ.
	/// sriq sriq.
	/// rA(), rS(), sh()	[rc()]
	sriqx,

	/// Shift right long immediate with MQ.
	/// srliq srliq.
	/// rA(), rS(), sh()	[rc()]
	srliqx,

	/// Shift right long with MQ.
	/// srlq srlq.
	/// rA(), rS(), rB()	[rc()]
	srlqx,

	/// Shidt right with MQ.
	/// srq srq.
	/// rA(), rS(), rB()	[rc()]
	srqx,

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

	/// Floating point absolute.
	/// fabs fabs.
	/// frD(), frB()	[rc()]
	fabsx,

	/// Floating point add.
	/// fadd fadd.
	/// frD(), frA(), frB()	[rc()]
	faddx,

	/// Floating point add single precision.
	/// fadds fadds.
	/// frD(), frA(), frB()	[rc()]
	faddsx,

	/// Floating point compare ordered.
	/// fcmpo
	/// crfD(), frA(), frB()
	fcmpo,

	/// Floating point compare unordered.
	/// fcmpu
	/// crfD(), frA(), frB()
	fcmpu,

	/// Floating point convert to integer word.
	/// fctiw fctiw.
	/// frD(), frB()	[rc()]
	fctiwx,

	/// Floating point convert to integer word with round towards zero.
	/// fctiw fctiw.
	/// frD(), frB()	[rc()]
	fctiwzx,

	/// Floating point divide.
	/// fdiv fdiv.
	/// frD(), frA(), frB()	[rc()]
	fdivx,

	/// Floating point divide single precision.
	/// fdiv fdiv.
	/// frD(), frA(), frB()	[rc()]
	fdivsx,

	/// Floating point multiply add.
	/// fmadd fmadd.
	/// frD(), frA(), frC(), frB()	[rc()]
	fmaddx,

	/// Floating point multiply add single precision.
	/// fmadds fmadds.
	/// frD(), frA(), frC(), frB()	[rc()]
	fmaddsx,

	/// Floating point register move.
	/// fmr fmr.
	/// frD(), frB()	[rc()]
	fmrx,

	/// Floating point multiply subtract.
	/// fmsub fmsub.
	/// frD(), frA(), frC(), frB()	[rc()]
	fmsubx,

	/// Floating point multiply subtract single precision.
	/// fmsubx fmsubx.
	/// frD(), frA(), frC(), frB()	[rc()]
	fmsubsx,

	/// Floating point multiply.
	/// fmul fmul.
	/// frD(), frA(), frC()	[rc()]
	fmulx,

	/// Floating point multiply single precision.
	/// fmuls fmuls.
	/// frD(), frA(), frC()	[rc()]
	fmulsx,

	/// Floating negative absolute value.
	/// fnabs fnabs.
	/// frD(), frB()	[rc()]
	fnabsx,

	/// Floating negative.
	/// fneg fneg.
	/// frD(), frB()	[rc()]
	fnegx,

	/// Floating point negative multiply add.
	/// fnmadd fnmadd.
	/// frD(), frA(), frC(), frB()	[rc()]
	fnmaddx,

	/// Floating point negative multiply add single precision.
	/// fnmadds fnmadds.
	/// frD(), frA(), frC(), frB()	[rc()]
	fnmaddsx,

	/// Floating point negative multiply subtract.
	/// fnmsub fnmsub.
	/// frD(), frA(), frC(), frB()	[rc()]
	fnmsubx,

	/// Floating point negative multiply add.
	/// fnmsubs fnmsubs.
	/// frD(), frA(), frC(), frB()	[rc()]
	fnmsubsx,

	/// Floating point round to single precision.
	/// frsp frsp.
	/// frD(), frB()	[rc()]
	frspx,

	/// Floating point subtract.
	/// fsub fsub.
	/// frD(), frA(), frB()	[rc()]
	fsubx,

	/// Floating point subtract single precision.
	/// fsubs fsubs.
	/// frD(), frA(), frB()	[rc()]
	fsubsx,

	/// Instruction cache block invalidate.
	/// icbi
	/// rA(), rB()
	icbi,

	/// Instruction synchronise.
	/// isync
	isync,

	/// Load byte and zero.
	/// lbz
	/// rD(), d() [ rA() ]
	lbz,

	/// Load byte and zero with update.
	/// lbz
	/// rD(), d() [ rA() ]
	lbzu,

	/// Load byte and zero with update indexed.
	/// lbzux
	/// rD(), rA(), rB()
	lbzux,

	/// Load byte and zero indexed.
	/// lbzx
	/// rD(), rA(), rB()
	lbzx,

	/// Load floating point double precision.
	/// lfd
	/// frD(), d() [ rA() ]
	lfd,

	/// Load floating point double precision with update.
	/// lfdu
	/// frD(), d() [ rA() ]
	lfdu,

	/// Load floating point double precision with update indexed.
	/// lfdux
	/// frD(), rA(), rB()
	lfdux,

	/// Load floating point double precision indexed.
	/// lfdx
	/// frD(), rA(), rB()
	lfdx,

	/// Load floating point single precision.
	/// lfs
	/// frD(), d() [ rA() ]
	lfs,

	/// Load floating point single precision with update.
	/// lfsu
	/// frD(), d() [ rA() ]
	lfsu,

	/// Load floating point single precision with update indexed.
	/// lfsux
	/// frD(), rA(), rB()
	lfsux,

	/// Load floating point single precision indexed.
	/// lfsx
	/// frD(), rA(), rB()
	lfsx,

	/// Load half word algebraic.
	/// lha
	/// rD(), d() [ rA() ]
	lha,

	/// Load half word algebraic with update.
	/// lha
	/// rD(), d() [ rA() ]
	lhau,

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
	/// rD(), d() [ rA() ]
	lhz,

	/// Load half-word and zero with update.
	/// lhzu
	/// rD(), d() [ rA() ]
	lhzu,

	/// Load half-word and zero with update indexed.
	/// lhzux
	/// rD(), rA(), rB()
	lhzux,

	/// Load half-word and zero indexed.
	/// lhzx
	/// rD(), rA(), rB()
	lhzx,

	/// Load multiple word.
	/// lmw
	/// rD(), d() [ rA() ]
	lmw,

	/// Load string word immediate.
	/// lswi
	/// rD(), rA(), nb()
	lswi,

	/// Load string word indexed.
	/// lswx
	/// rD(), rA(), rB()
	lswx,

	/// Load word and reserve indexed.
	/// lwarx
	/// rD(), rA(), rB()
	lwarx,

	/// Load word byte-reverse indexed.
	/// lwbrx
	/// rD(), rA(), rB()
	lwbrx,

	/// Load word and zero.
	/// lwz
	/// rD(), d() [ rA() ]
	lwz,

	/// Load word and zero with update.
	/// lwzu
	/// rD(), d() [ rA() ]
	lwzu,

	/// Load word and zero with update indexed.
	/// lwzux
	lwzux,

	/// Load word and zero indexed.
	/// lwzx
	lwzx,

	/// Move condition register field.
	/// mcrf
	/// crfD(), crfS()
	mcrf,

	/// Move to condition register from FPSCR.
	/// mcrfs
	/// crfD(), crfS()
	mcrfs,

	/// Move to condition register from XER.
	/// mcrxr
	/// crfD()
	mcrxr,

	/// Move from condition register.
	/// mfcr
	/// rD()
	mfcr,

	/// Move from FPSCR.
	/// mffs mffs.
	/// frD()	[rc()]
	mffsx,

	/// Move from machine state register.
	/// mfmsr
	/// rD()
	mfmsr,

	/// Move from special purpose record.
	/// mfmsr
	/// rD(), spr()
	mfspr,

	/// Move from segment register.
	/// mfsr
	/// rD(), sr()
	mfsr,

	/// Move from segment register indirect.
	/// mfsrin
	/// rD(), rB()
	mfsrin,

	/// Move to condition register fields.
	/// mtcrf
	/// rS(), crm()
	mtcrf,

	/// Move to FPSCR bit 0.
	/// mtfsb0 mtfsb0.
	/// crbD()
	mtfsb0x,

	/// Move to FPSCR bit 1.
	/// mtfsb1 mtfsb1.
	/// crbD()
	mtfsb1x,

	/// Move to FPSCR fields.
	/// mtfsf mtfsf.
	/// fm(), frB()	[rc()]
	mtfsfx,

	/// Move to FPSCR field immediate.
	/// mtfsfi mtfsfi.
	/// crfD(), imm()
	mtfsfix,

	/// Move to FPSCR.
	/// mtfs
	/// frS()
	mtmsr,

	/// Move to special purpose record.
	/// mtmsr
	/// rD(), spr()
	mtspr,

	/// Move to segment register.
	/// mfsr
	/// sr(), rS()
	mtsr,

	/// Move to segment register indirect.
	/// mtsrin
	/// rD(), rB()
	mtsrin,

	/// Multiply high word.
	/// mulhw mulgw.
	/// rD(), rA(), rB(), rc()
	mulhwx,

	/// Multiply high word unsigned.
	/// mulhwu mulhwu.
	/// rD(), rA(), rB(), rc()
	mulhwux,

	/// Multiply low immediate.
	/// mulli
	/// rD(), rA(), simm()
	mulli,

	/// Multiply low word.
	/// mullw mullw. mullwo mullwo.
	/// rA(), rB(), rD()
	mullwx,

	/// NAND.
	/// nand nand.
	/// rA(), rS(), rB()	[rc()]
	nandx,

	/// 'Negate' (negative).
	/// neg neg. nego nego.
	/// rD(), rA()	[rc(), oe()]
	negx,

	/// NOR
	/// nor nor.
	/// rA(), rS(), rB()	[rc()]
	norx,

	/// OR.
	/// or or.
	/// rA(), rS(), rB()	[rc()]
	orx,

	/// OR with complement.
	/// orc orc.
	/// rA(), rS(), rB()	[rc()]
	orcx,

	/// OR immediate.
	/// ori
	/// rA(), rS(), uimm()
	ori,

	/// OR immediate shifted.
	/// oris
	/// rA(), rS(), uimm()
	oris,

	/// Return from interrupt.
	/// rfi
	rfi,

	/// Rotate left word immediate then mask insert.
	/// rlwimi rlwimi.
	/// rA(), rS(), sh(), mb(), me()	[rc()]
	/// Cf. rotate_mask()
	rlwimix,

	/// Rotate left word immediate then AND with mask.
	/// rlwinm rlwinm.
	/// rA(), rS(), sh(), mb(), me()	[rc()]
	/// Cf. rotate_mask()
	rlwinmx,

	/// Rotate left word then AND with mask
	/// rlwimi rlwimi.
	/// rA(), rB(), rS(), mb(), me()	[rc()]
	/// Cf. rotate_mask()
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

	/// Store byte indexed.
	/// stbx
	/// rS(), rA(), rB()
	stbx,

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
	/// frS(), d() [ rA() ]
	stfs,

	/// Store floating point single precision with update.
	/// stfsu
	/// frS(), d() [ rA() ]
	stfsu,

	/// Store floating point single precision with update indexed.
	/// stfsux
	/// frS(), rA(), rB()
	stfsux,

	/// Store floating point single precisionindexed.
	/// stfsx
	/// frS(), rA(), rB()
	stfsx,

	/// Store half word.
	/// sth
	/// rS(), d() [ rA() ]
	sth,

	/// Store half word byte-reverse indexed.
	/// sthbrx
	/// rS(), rA(), rB()
	sthbrx,

	/// Store half word with update.
	/// sthu
	/// rS(), d() [ rA() ]
	sthu,

	/// Store half-word with update indexed.
	/// sthux
	/// rS(), rA(), rB()
	sthux,

	/// Store half-word indexed.
	/// sthx
	/// rS(), rA(), rB()
	sthx,

	/// Store multiple word.
	/// stmw
	/// rS(), d() [ rA() ]
	stmw,

	/// Store string word immediate.
	/// stswi
	/// rS(), rA(), nb()
	stswi,

	/// Store string word indexed.
	/// stswx
	/// rS(), rA(), rB()
	stswx,

	/// Store word.
	/// stw
	/// rS(), d() [ rA() ]
	stw,

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

	/// Subtract from.
	/// subf subf. subfo subfo.
	/// rD(), rA(), rB()	[rc(), oe()]
	subfx,

	/// Subtract from carrying.
	/// subfc subfc. subfco subfco.
	/// rD(), rA(), rB()	[rc(), oe()]
	subfcx,

	/// Subtract from extended.
	/// subfe subfe. subfeo subfeo.
	/// rD(), rA(), rB()	[rc(), oe()]
	subfex,

	/// Subtract from immediate carrying
	/// subfic
	/// rD(), rA(), simm()
	subfic,

	/// Subtract from minus one extended.
	/// subfme subfme. subfmeo subfmeo.
	/// rD(), rA()	[rc(), oe()]
	subfmex,

	/// Subtract from zero extended.
	/// subfze subfze. subfzeo subfzeo.
	/// rD(), rA()	[rc(), oe()]
	subfzex,

	/// Synchronise.
	/// sync
	sync,

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

	/// Translation lookaside buffer ('TLB') invalidate all.
	/// tlbia
	tlbia,

	/// Translation lookaside buffer ('TLB') invalidate entry.
	/// tlbie
	/// rB()
	tlbie,

	/// Translation lookaside buffer ('TLB') synchronise.
	/// tlbsync
	tlbsync,

	//
	// MARK: - Optional.
	//

	/// Move from time base.
	/// mftb
	/// rD(), tbr()
	mftb,

	/// Floaring point reciprocal estimate single precision.
	/// fres fres.
	/// frD(), frB()	[rc()]
	fresx,

	/// Floating point reciprocal square root estimation.
	/// frsqrte frsqrte.
	/// frD(), frB()	[rc()]
	frsqrtex,

	/// Floating point select.
	/// fsel fsel.
	/// frD(), frA(), frC(), frB()	[rc()]
	fselx,

	/// Floating Point square root.
	/// fsqrt fsqrt.
	/// frD(), frB()	[rc()]
	fsqrtx,

	/// Floating point square root single precision.
	/// fsqrts fsqrts.
	/// frD(), frB()	[rc()]
	fsqrtsx,

	/// Store floating point as integer word indexed.
	/// stfiwx
	/// frS(), rA(), rB()
	stfiwx,

	//
	// MARK: - 64-bit only PowerPC instructions.
	//

	/// Count leading zero double word.
	/// cntlzd cntlzd.
	/// rA(), rS()	[rc()]
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

	/// Floating point convert from integer double word.
	/// fcfid fcfid.
	/// frD(), frB()	[rc()]
	fcfidx,

	/// Floating point convert to integer double word.
	/// fctid fctid.
	/// frD(), frB()	[rc()]
	fctidx,

	/// Floating point convert to integer double word with round towards zero.
	/// fctid fctid.
	/// frD(), frB()	[rc()]
	fctidzx,

	/// Load double word.
	/// ld
	/// rD(), ds() [ rA() ]
	ld,

	/// Load double word and reserve indexed.
	/// ldarx
	/// rD(), rA(), rB()
	ldarx,

	/// Load double word with update.
	/// ldu
	/// rD(), ds() [ rA() ]
	ldu,

	/// Load double word with update indexed.
	/// ldux
	/// rD(), rA(), rB()
	ldux,

	/// Load double word indexed.
	/// ldx
	/// rD(), rA(), rB()
	ldx,

	/// Load word algebraic.
	/// lwa
	/// rD(), ds() [ rA() ]
	lwa,

	/// Load word algebraic with update indexed.
	/// lwaux
	/// rD(), rA(), rB()
	lwaux,

	/// Load word algebraic indexed.
	/// lwax
	/// rD(), rA(), rB()
	lwax,

	/// Multiply high double word.
	/// mulhd mulhd.
	/// rD(), rA(), rB()	[rc()]
	mulhdx,

	/// Multiply high double word unsigned.
	/// mulhdy mulhdu.
	/// rD(), rA(), rB()	[rc()]
	mulhdux,

	/// Multiply low double word.
	/// mulld mulld. mulldo mulldo.
	/// rD(), rA(), rB()	[rc()]
	mulldx,

	/// Rotate left double word then clear left.
	/// rldcl rldcl.
	/// rA(), rS(), rB(), mb<uint64_t>()	[rc()]
	rldclx,

	/// Rotate left double word then clear right.
	/// rldcr rldcr.
	/// rA(), rS(), rB(), mb<uint64_t>()	[rc()]
	rldcrx,

	/// Rotate left double word then clear.
	/// rldic rldic.
	/// rA(), rS(), rB(), sh<uint64_t>(), mb<uint64_t>()	[rc()]
	rldicx,

	/// Rotate left double word then clear left.
	/// rldicl rldicl.
	/// rA(), rS(), rB(), sh<uint64_t>(), mb<uint64_t>()	[rc()]
	rldiclx,

	/// Rotate left double word then clear right.
	/// rldicr rldicr.
	/// rA(), rS(), rB(), sh<uint64_t>(), me<uint64_t>()	[rc()]
	rldicrx,

	/// Rotate left double word immediate then mask insert.
	/// rldiml rldimi.
	/// rA(), rS(), rB(), sh<uint64_t>(), mb<uint64_t>()	[rc()]
	rldimix,

	/// Segment lookaside buffer ('SLB') invalidate all.
	/// slbia
	slbia,

	/// Segment lookaside buffer ('SLB') invalidate entry.
	/// slbie
	/// rB()
	slbie,

	/// Shift left double word.
	/// sld sld.
	/// rA(), rS(), rB()
	sldx,

	/// Shift right algebraic double word.
	/// srad srad,
	/// rA(), rS(), rB()	[rc()]
	sradx,

	/// Shift right algebraic double word immediate.
	/// sradi sradi.
	/// rA(), rS(),sh<uint64_t>()	[rc()]
	sradix,

	/// Shift right double word.
	/// srd srd.
	/// rA(), rS(), rB()	[rc()]
	srdx,

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

	/// Trap double word.
	/// td
	/// to(), rA(), rB()
	td,

	/// Trap double word immediate.
	/// tdi
	/// to(), rA(), simm()
	tdi,
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

	/// Specifies the first 1 bit of a 32-bit mask for 32-bit rotate operations.
	template <typename IntT = uint32_t> IntT mb() const {
		if constexpr (sizeof(IntT) == 4) {
			return (opcode >> 6) & 0x1f;
		} else {
			return (opcode >> 5) & 0x3f;
		}
	}
	/// Specifies the first 1 bit of a 32/64-bit mask for rotate operations.
	/// Specify IntT as uint32_t for the 32-bit rotate instructions, uint64_t for the 64-bit.
	template <typename IntT = uint32_t> IntT me() const {
		if constexpr (sizeof(IntT) == 4) {
			return (opcode >> 1) & 0x1f;
		} else {
			return (opcode >> 5) & 0x3f;
		}
	}

	/// Provides the mask described by 32-bit rotate operations.
	///
	/// Per IBM's rules:
	/// 	mb < me+1 	=> set [mb, me]
	/// 	mb == me+1	=> set all bits
	/// 	mb > me+1	=> complement of set [me+1, mb-1]
	template <typename IntT> IntT rotate_mask() const {
		const auto mb_bit = mb();
		const auto me_bit = me();

		const IntT result = (0xffff'ffff >> mb_bit) ^ (0x7fff'ffff >> me_bit);
		const IntT sign = ~IntT((int32_t(mb_bit) - int32_t(me_bit+1)) >> 16);
		return result ^ sign;
	}

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
	uint32_t nb() const		{
		// Map nb == 0 to 32, branchlessly, given that this is
		// a five-bit field.
		const uint32_t nb = (opcode >> 11) & 0x1f;
		return ((nb - 1) & 31) + 1;
	}

	/// Specifies a shift amount. Use IntT = uint32_t to get the shift value embedded in
	/// 32-bit instructions, uint64_t for 64-bit instructions.
	template <typename IntT = uint32_t> uint32_t sh() const {
		uint32_t sh = (opcode >> 11) & 0x1f;
		if constexpr (sizeof(IntT) == 8) {
			sh |= (opcode & 2) << 4;
		}
		return sh;
	}

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


	/// Identifies a special purpose register.
	uint32_t spr() const 	{	return (opcode >> 11) & 0x3ff;		}
	/// Identifies a time base register.
	uint32_t tbr() const 	{	return (opcode >> 11) & 0x3ff;		}
};

// Sanity check on Instruction size.
static_assert(sizeof(Instruction) <= 8);

}
}

#endif /* InstructionSets_PowerPC_Instruction_h */
