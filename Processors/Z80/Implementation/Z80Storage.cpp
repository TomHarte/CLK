//
//  Z80Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "../Z80.hpp"
#include <cstring>

using namespace CPU::Z80;

ProcessorStorage::ProcessorStorage() {
	set_flags(0xff);
}

// Elemental bus operations
#define ReadOpcodeStart()			PartialMachineCycle(PartialMachineCycle::ReadOpcodeStart, HalfCycles(3), &pc_.full, &operation_, false)
#define ReadOpcodeWait(f)			PartialMachineCycle(PartialMachineCycle::ReadOpcodeWait, HalfCycles(2), &pc_.full, &operation_, f)
#define ReadOpcodeEnd()				PartialMachineCycle(PartialMachineCycle::ReadOpcode, HalfCycles(1), &pc_.full, &operation_, false)

#define Refresh()					PartialMachineCycle(PartialMachineCycle::Refresh, HalfCycles(4), &refresh_addr_.full, nullptr, false)

#define ReadStart(addr, val)		PartialMachineCycle(PartialMachineCycle::ReadStart, HalfCycles(3), &addr.full, &val, false)
#define ReadWait(l, addr, val, f)	PartialMachineCycle(PartialMachineCycle::ReadWait, HalfCycles(l), &addr.full, &val, f)
#define ReadEnd(addr, val)			PartialMachineCycle(PartialMachineCycle::Read, HalfCycles(3), &addr.full, &val, false)

#define WriteStart(addr, val)		PartialMachineCycle(PartialMachineCycle::WriteStart,HalfCycles(3), &addr.full, &val, false)
#define WriteWait(l, addr, val, f)	PartialMachineCycle(PartialMachineCycle::WriteWait, HalfCycles(l), &addr.full, &val, f)
#define WriteEnd(addr, val)			PartialMachineCycle(PartialMachineCycle::Write, HalfCycles(3), &addr.full, &val, false)

#define InputStart(addr, val)		PartialMachineCycle(PartialMachineCycle::InputStart, HalfCycles(3), &addr.full, &val, false)
#define InputWait(addr, val, f)		PartialMachineCycle(PartialMachineCycle::InputWait, HalfCycles(2), &addr.full, &val, f)
#define InputEnd(addr, val)			PartialMachineCycle(PartialMachineCycle::Input, HalfCycles(3), &addr.full, &val, false)

#define OutputStart(addr, val)		PartialMachineCycle(PartialMachineCycle::OutputStart, HalfCycles(3), &addr.full, &val, false)
#define OutputWait(addr, val, f)	PartialMachineCycle(PartialMachineCycle::OutputWait, HalfCycles(2), &addr.full, &val, f)
#define OutputEnd(addr, val)		PartialMachineCycle(PartialMachineCycle::Output, HalfCycles(3), &addr.full, &val, false)

#define IntAckStart(length, val)	PartialMachineCycle(PartialMachineCycle::InterruptStart, HalfCycles(length), nullptr, &val, false)
#define IntWait(val)				PartialMachineCycle(PartialMachineCycle::InterruptWait, HalfCycles(2), nullptr, &val, true)
#define IntAckEnd(val)				PartialMachineCycle(PartialMachineCycle::Interrupt, HalfCycles(3), nullptr, &val, false)


// A wrapper to express a bus operation as a micro-op
#define BusOp(op)					{MicroOp::BusOperation, nullptr, nullptr, op}

// Compound bus operations, as micro-ops

#define InternalOperation(len)		{MicroOp::BusOperation, nullptr, nullptr, {PartialMachineCycle::Internal, HalfCycles(len), &last_address_bus_, nullptr, false}}

// Read3 is a standard read cycle: 1.5 cycles, then check the wait line, then 1.5 cycles;
// Read4 is a four-cycle read that has to do something to calculate the address: 1.5 cycles, then an extra wait cycle, then check the wait line, then 1.5 cycles;
// Read4Pre is a four-cycle read that has to do something after reading: 1.5 cycles, then check the wait line, then 1.5 cycles, then a 1-cycle internal operation;
// Read5 is a five-cycle read: 1.5 cycles, two wait cycles, check the wait line, 1.5 cycles.
#define Read3(addr, val)			BusOp(ReadStart(addr, val)), BusOp(ReadWait(2, addr, val, true)), BusOp(ReadEnd(addr, val))
#define Read4(addr, val)			BusOp(ReadStart(addr, val)), BusOp(ReadWait(2, addr, val, false)), BusOp(ReadWait(2, addr, val, true)), BusOp(ReadEnd(addr, val))
#define Read4Pre(addr, val)			BusOp(ReadStart(addr, val)), BusOp(ReadWait(2, addr, val, true)), BusOp(ReadEnd(addr, val)), InternalOperation(2)
#define Read5(addr, val)			BusOp(ReadStart(addr, val)), BusOp(ReadWait(4, addr, val, false)), BusOp(ReadWait(2, addr, val, true)), BusOp(ReadEnd(addr, val))

#define Write3(addr, val)			BusOp(WriteStart(addr, val)), BusOp(WriteWait(2, addr, val, true)), BusOp(WriteEnd(addr, val))
#define Write5(addr, val)			BusOp(WriteStart(addr, val)), BusOp(WriteWait(4, addr, val, false)), BusOp(WriteWait(2, addr, val, true)), BusOp(WriteEnd(addr, val))

#define Input(addr, val)			BusOp(InputStart(addr, val)), BusOp(InputWait(addr, val, false)), BusOp(InputWait(addr, val, true)), BusOp(InputEnd(addr, val))
#define Output(addr, val)			BusOp(OutputStart(addr, val)), BusOp(OutputWait(addr, val, false)), BusOp(OutputWait(addr, val, true)), BusOp(OutputEnd(addr, val))

/// A sequence is a series of micro-ops that ends in a move-to-next-program operation.
#define Sequence(...)				{ __VA_ARGS__, {MicroOp::MoveToNextProgram} }

// Assumption made: those instructions that are rated with an opcode fetch greater than four cycles spend the extra time
// providing a lengthened refresh cycle. I assume this because the CPU doesn't have foresight and presumably spends the
// normal refresh time decoding. So if it gets to cycle four and realises it has two more cycles of work, I have assumed
// it simply maintains the refresh state for an extra two cycles.

/* The following are helper macros that define common parts of instructions */
#define Inc16(r)				{(&r == &pc_) ? MicroOp::IncrementPC : MicroOp::Increment16, &r.full}
#define Inc8NoFlags(r)			{MicroOp::Increment8NoFlags, &r}

#define ReadInc(addr, val)		Read3(addr, val), Inc16(addr)
#define Read4Inc(addr, val)		Read4(addr, val), Inc16(addr)
#define Read5Inc(addr, val)		Read5(addr, val), Inc16(addr)
#define WriteInc(addr, val)		Write3(addr, val), {MicroOp::Increment16, &addr.full}

#define Read16Inc(addr, val)	ReadInc(addr, val.halves.low), ReadInc(addr, val.halves.high)
#define Read16(addr, val)		ReadInc(addr, val.halves.low), Read3(addr, val.halves.high)

#define Write16(addr, val)		WriteInc(addr, val.halves.low), Write3(addr, val.halves.high)

#define INDEX()					{MicroOp::IndexedPlaceHolder}, ReadInc(pc_, temp8_), InternalOperation(10), {MicroOp::CalculateIndexAddress, &index}
#define FINDEX()				{MicroOp::IndexedPlaceHolder}, ReadInc(pc_, temp8_), {MicroOp::CalculateIndexAddress, &index}
#define INDEX_ADDR()			(add_offsets ? memptr_ : index)

#define Push(x)					{MicroOp::Decrement16, &sp_.full}, Write3(sp_, x.halves.high), {MicroOp::Decrement16, &sp_.full}, Write3(sp_, x.halves.low)
#define Pop(x)					Read3(sp_, x.halves.low), {MicroOp::Increment16, &sp_.full}, Read3(sp_, x.halves.high), {MicroOp::Increment16, &sp_.full}

#define Push8(x)				{MicroOp::Decrement16, &sp_.full}, Write3(sp_, x.halves.high), {MicroOp::Decrement16, &sp_.full}, Write5(sp_, x.halves.low)
#define Pop7(x)					Read3(sp_, x.halves.low), {MicroOp::Increment16, &sp_.full}, Read4(sp_, x.halves.high), {MicroOp::Increment16, &sp_.full}

/* The following are actual instructions */
#define NOP						{ {MicroOp::MoveToNextProgram} }

#define JP(cc)					Sequence(Read16Inc(pc_, memptr_), {MicroOp::cc, nullptr}, {MicroOp::Move16, &memptr_.full, &pc_.full})
#define CALL(cc)				Sequence(ReadInc(pc_, memptr_.halves.low), {MicroOp::cc, conditional_call_untaken_program_.data()}, Read4Inc(pc_, memptr_.halves.high), Push(pc_), {MicroOp::Move16, &memptr_.full, &pc_.full})
#define RET(cc)					Sequence(InternalOperation(2), {MicroOp::cc, nullptr}, Pop(memptr_), {MicroOp::Move16, &memptr_.full, &pc_.full})
#define JR(cc)					Sequence(ReadInc(pc_, temp8_), {MicroOp::cc, nullptr}, InternalOperation(10), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &memptr_.full, &pc_.full})
#define RST()					Sequence(InternalOperation(2), {MicroOp::CalculateRSTDestination}, Push(pc_), {MicroOp::Move16, &memptr_.full, &pc_.full})
#define LD(a, b)				Sequence({MicroOp::Move8, &b, &a})

#define LD_GROUP(r, ri)	\
				LD(r, bc_.halves.high),		LD(r, bc_.halves.low),		LD(r, de_.halves.high),		LD(r, de_.halves.low),	\
				LD(r, index.halves.high),	LD(r, index.halves.low),		\
				Sequence(INDEX(), Read3(INDEX_ADDR(), temp8_), {MicroOp::Move8, &temp8_, &ri}),		\
				LD(r, a_)

#define READ_OP_GROUP(op)	\
				Sequence({MicroOp::op, &bc_.halves.high}),		Sequence({MicroOp::op, &bc_.halves.low}),	\
				Sequence({MicroOp::op, &de_.halves.high}),		Sequence({MicroOp::op, &de_.halves.low}),	\
				Sequence({MicroOp::op, &index.halves.high}),	Sequence({MicroOp::op, &index.halves.low}),	\
				Sequence(INDEX(), Read3(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence({MicroOp::op, &a_})

#define READ_OP_GROUP_D(op)	\
				Sequence({MicroOp::op, &bc_.halves.high}),		Sequence({MicroOp::op, &bc_.halves.low}),	\
				Sequence({MicroOp::op, &de_.halves.high}),		Sequence({MicroOp::op, &de_.halves.low}),	\
				Sequence({MicroOp::op, &index.halves.high}),	Sequence({MicroOp::op, &index.halves.low}),	\
				Sequence(INDEX(), Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence({MicroOp::op, &a_})

#define RMW(x, op, ...) Sequence(INDEX(), Read4Pre(INDEX_ADDR(), x), {MicroOp::op, &x}, Write3(INDEX_ADDR(), x))
#define RMWI(x, op, ...) Sequence(Read4(INDEX_ADDR(), x), {MicroOp::op, &x}, Write3(INDEX_ADDR(), x))

#define MODIFY_OP_GROUP(op)	\
				Sequence({MicroOp::op, &bc_.halves.high}),		Sequence({MicroOp::op, &bc_.halves.low}),	\
				Sequence({MicroOp::op, &de_.halves.high}),		Sequence({MicroOp::op, &de_.halves.low}),	\
				Sequence({MicroOp::op, &index.halves.high}),	Sequence({MicroOp::op, &index.halves.low}),	\
				RMW(temp8_, op),	\
				Sequence({MicroOp::op, &a_})

#define IX_MODIFY_OP_GROUP(op)	\
				RMWI(bc_.halves.high, op),	\
				RMWI(bc_.halves.low, op),	\
				RMWI(de_.halves.high, op),	\
				RMWI(de_.halves.low, op),	\
				RMWI(hl_.halves.high, op),	\
				RMWI(hl_.halves.low, op),	\
				RMWI(temp8_, op),	\
				RMWI(a_, op)

#define IX_READ_OP_GROUP(op)	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				Sequence(Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_})

#define ADD16(d, s) Sequence(InternalOperation(8), InternalOperation(6), {MicroOp::ADD16, &s.full, &d.full})
#define ADC16(d, s) Sequence(InternalOperation(8), InternalOperation(6), {MicroOp::ADC16, &s.full, &d.full})
#define SBC16(d, s) Sequence(InternalOperation(8), InternalOperation(6), {MicroOp::SBC16, &s.full, &d.full})

void ProcessorStorage::install_default_instruction_set() {
	MicroOp conditional_call_untaken_program[] = Sequence(ReadInc(pc_, memptr_.halves.high));
	copy_program(conditional_call_untaken_program, conditional_call_untaken_program_);

	assemble_base_page(base_page_, hl_, false, cb_page_);
	assemble_base_page(dd_page_, ix_, true, ddcb_page_);
	assemble_base_page(fd_page_, iy_, true, fdcb_page_);
	assemble_ed_page(ed_page_);

	fdcb_page_.r_step = 0;
	fd_page_.is_indexed = true;
	fdcb_page_.is_indexed = true;

	ddcb_page_.r_step = 0;
	dd_page_.is_indexed = true;
	ddcb_page_.is_indexed = true;

	assemble_fetch_decode_execute(base_page_, 4);
	assemble_fetch_decode_execute(dd_page_, 4);
	assemble_fetch_decode_execute(fd_page_, 4);
	assemble_fetch_decode_execute(ed_page_, 4);
	assemble_fetch_decode_execute(cb_page_, 4);

	assemble_fetch_decode_execute(fdcb_page_, 3);
	assemble_fetch_decode_execute(ddcb_page_, 3);

	MicroOp reset_program[] = Sequence(InternalOperation(6), {MicroOp::Reset});

	// Justification for NMI timing: per Wilf Rigter on the ZX81 (http://www.user.dccnet.com/wrigter/index_files/ZX81WAIT.htm),
	// wait cycles occur between T2 and T3 during NMI; extending the refresh cycle is also consistent with my guess
	// for the action of other non-four-cycle opcode fetches
	MicroOp nmi_program[] = {
		{ MicroOp::BeginNMI },
		BusOp(ReadOpcodeStart()),
		BusOp(ReadOpcodeWait(true)),
		BusOp(ReadOpcodeEnd()),
		BusOp(Refresh()),
		InternalOperation(2),
		Push(pc_),
		{ MicroOp::JumpTo66, nullptr, nullptr},
		{ MicroOp::MoveToNextProgram }
	};
	MicroOp irq_mode0_program[] = {
		{ MicroOp::BeginIRQMode0 },
		BusOp(IntAckStart(5, operation_)),
		BusOp(IntWait(operation_)),
		BusOp(IntAckEnd(operation_)),
		{ MicroOp::DecodeOperation }
	};
	MicroOp irq_mode1_program[] = {
		{ MicroOp::BeginIRQ },
		BusOp(IntAckStart(7, operation_)),	// 7 half cycles (including  +
		BusOp(IntWait(operation_)),			// [potentially 2 half cycles] +
		BusOp(IntAckEnd(operation_)),		// Implicitly 3 half cycles +
		BusOp(Refresh()),					// 4 half cycles +
		Push(pc_),							// 12 half cycles = 26 half cycles = 13 cycles
		{ MicroOp::Move16, &temp16_.full, &pc_.full },
		{ MicroOp::MoveToNextProgram }
	};
	MicroOp irq_mode2_program[] = {
		{ MicroOp::BeginIRQ },
		BusOp(IntAckStart(7, temp16_.halves.low)),
		BusOp(IntWait(temp16_.halves.low)),
		BusOp(IntAckEnd(temp16_.halves.low)),
		BusOp(Refresh()),
		Push(pc_),
		{ MicroOp::Move8, &ir_.halves.high, &temp16_.halves.high },
		Read16(temp16_, pc_),
		{ MicroOp::MoveToNextProgram }
	};

	copy_program(reset_program, reset_program_);
	copy_program(nmi_program, nmi_program_);
	copy_program(irq_mode0_program, irq_program_[0]);
	copy_program(irq_mode1_program, irq_program_[1]);
	copy_program(irq_mode2_program, irq_program_[2]);
}

void ProcessorStorage::assemble_ed_page(InstructionPage &target) {
#define IN_C(r)		Sequence({MicroOp::Move16, &bc_.full, &memptr_.full}, Input(bc_, r), {MicroOp::SetInFlags, &r})
#define OUT_C(r)	Sequence(Output(bc_, r), {MicroOp::SetOutFlags})
#define IN_OUT(r)	IN_C(r), OUT_C(r)

#define NOP_ROW()	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP
	InstructionTable ed_program_table = {
		NOP_ROW(),	/* 0x00 */
		NOP_ROW(),	/* 0x10 */
		NOP_ROW(),	/* 0x20 */
		NOP_ROW(),	/* 0x30 */
		/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(bc_.halves.high),
		/* 0x42 SBC HL, BC */	SBC16(hl_, bc_),				/* 0x43 LD (nn), BC */	Sequence(Read16Inc(pc_, memptr_), Write16(memptr_, bc_)),
		/* 0x44 NEG */			Sequence({MicroOp::NEG}),		/* 0x45 RETN */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x46 IM 0 */			Sequence({MicroOp::IM}),		/* 0x47 LD I, A */		Sequence(InternalOperation(2), {MicroOp::Move8, &a_, &ir_.halves.high}),
		/* 0x48 IN C, (C);	0x49 OUT (C), C */	IN_OUT(bc_.halves.low),
		/* 0x4a ADC HL, BC */	ADC16(hl_, bc_),				/* 0x4b LD BC, (nn) */	Sequence(Read16Inc(pc_, memptr_), Read16(memptr_, bc_)),
		/* 0x4c NEG */			Sequence({MicroOp::NEG}),		/* 0x4d RETI */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x4e IM 0/1 */		Sequence({MicroOp::IM}),		/* 0x4f LD R, A */		Sequence(InternalOperation(2), {MicroOp::Move8, &a_, &ir_.halves.low}),
		/* 0x50 IN D, (C);	0x51 OUT (C), D */	IN_OUT(de_.halves.high),
		/* 0x52 SBC HL, DE */	SBC16(hl_, de_),				/* 0x53 LD (nn), DE */	Sequence(Read16Inc(pc_, memptr_), Write16(memptr_, de_)),
		/* 0x54 NEG */			Sequence({MicroOp::NEG}),		/* 0x55 RETN */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x56 IM 1 */			Sequence({MicroOp::IM}),		/* 0x57 LD A, I */		Sequence(InternalOperation(2), {MicroOp::Move8, &ir_.halves.high, &a_}, {MicroOp::SetAFlags}),
		/* 0x58 IN E, (C);	0x59 OUT (C), E */	IN_OUT(de_.halves.low),
		/* 0x5a ADC HL, DE */	ADC16(hl_, de_),				/* 0x5b LD DE, (nn) */	Sequence(Read16Inc(pc_, memptr_), Read16(memptr_, de_)),
		/* 0x5c NEG */			Sequence({MicroOp::NEG}),		/* 0x5d RETN */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x5e IM 2 */			Sequence({MicroOp::IM}),		/* 0x5f LD A, R */		Sequence(InternalOperation(2), {MicroOp::Move8, &ir_.halves.low, &a_}, {MicroOp::SetAFlags}),
		/* 0x60 IN H, (C);	0x61 OUT (C), H */	IN_OUT(hl_.halves.high),
		/* 0x62 SBC HL, HL */	SBC16(hl_, hl_),				/* 0x63 LD (nn), HL */	Sequence(Read16Inc(pc_, memptr_), Write16(memptr_, hl_)),
		/* 0x64 NEG */			Sequence({MicroOp::NEG}),		/* 0x65 RETN */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x66 IM 0 */			Sequence({MicroOp::IM}),		/* 0x67 RRD */			Sequence(Read3(hl_, temp8_), InternalOperation(8), {MicroOp::RRD}, Write3(hl_, temp8_)),
		/* 0x68 IN L, (C);	0x69 OUT (C), L */	IN_OUT(hl_.halves.low),
		/* 0x6a ADC HL, HL */	ADC16(hl_, hl_),				/* 0x6b LD HL, (nn) */	Sequence(Read16Inc(pc_, memptr_), Read16(memptr_, hl_)),
		/* 0x6c NEG */			Sequence({MicroOp::NEG}),		/* 0x6d RETN */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x6e IM 0/1 */		Sequence({MicroOp::IM}),		/* 0x6f RLD */			Sequence(Read3(hl_, temp8_), InternalOperation(8), {MicroOp::RLD}, Write3(hl_, temp8_)),
		/* 0x70 IN (C) */		IN_C(temp8_),					/* 0x71 OUT (C), 0 */	Sequence({MicroOp::SetZero}, Output(bc_, temp8_), {MicroOp::SetOutFlags}),
		/* 0x72 SBC HL, SP */	SBC16(hl_, sp_),				/* 0x73 LD (nn), SP */	Sequence(Read16Inc(pc_, memptr_), Write16(memptr_, sp_)),
		/* 0x74 NEG */			Sequence({MicroOp::NEG}),		/* 0x75 RETN */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x76 IM 1 */			Sequence({MicroOp::IM}),		/* 0x77 XX */			NOP,
		/* 0x78 IN A, (C);	0x79 OUT (C), A */	IN_OUT(a_),
		/* 0x7a ADC HL, SP */	ADC16(hl_, sp_),				/* 0x7b LD SP, (nn) */	Sequence(Read16Inc(pc_, memptr_), Read16(memptr_, sp_)),
		/* 0x7c NEG */			Sequence({MicroOp::NEG}),		/* 0x7d RETN */			Sequence(Pop(pc_), {MicroOp::RETN}),
		/* 0x7e IM 2 */			Sequence({MicroOp::IM}),		/* 0x7f XX */			NOP,
		NOP_ROW(),	/* 0x80 ... 0x8f */
		NOP_ROW(),	/* 0x90 ... 0x9f */
		/* 0xa0 LDI */		Sequence(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDI}),
		/* 0xa1 CPI */		Sequence(Read3(hl_, temp8_), InternalOperation(10), {MicroOp::CPI}),
		/* 0xa2 INI */		Sequence(InternalOperation(2), Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::INI}),
		/* 0xa3 OTI */		Sequence(InternalOperation(2), Read3(hl_, temp8_), {MicroOp::OUTI}, Output(bc_, temp8_)),
		NOP, NOP, NOP, NOP,
		/* 0xa8 LDD */		Sequence(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDD}),
		/* 0xa9 CPD */		Sequence(Read3(hl_, temp8_), InternalOperation(10), {MicroOp::CPD}),
		/* 0xaa IND */		Sequence(InternalOperation(2), Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::IND}),
		/* 0xab OTD */		Sequence(InternalOperation(2), Read3(hl_, temp8_), {MicroOp::OUTD}, Output(bc_, temp8_)),
		NOP, NOP, NOP, NOP,
		/* 0xb0 LDIR */		Sequence(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDIR}, InternalOperation(10)),
		/* 0xb1 CPIR */		Sequence(Read3(hl_, temp8_), InternalOperation(10), {MicroOp::CPIR}, InternalOperation(10)),
		/* 0xb2 INIR */		Sequence(InternalOperation(2), Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::INIR}, InternalOperation(10)),
		/* 0xb3 OTIR */		Sequence(InternalOperation(2), Read3(hl_, temp8_), {MicroOp::OUTI}, Output(bc_, temp8_), {MicroOp::OUT_R}, InternalOperation(10)),
		NOP, NOP, NOP, NOP,
		/* 0xb8 LDDR */		Sequence(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDDR}, InternalOperation(10)),
		/* 0xb9 CPDR */		Sequence(Read3(hl_, temp8_), InternalOperation(10), {MicroOp::CPDR}, InternalOperation(10)),
		/* 0xba INDR */		Sequence(InternalOperation(2), Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::INDR}, InternalOperation(10)),
		/* 0xbb OTDR */		Sequence(InternalOperation(2), Read3(hl_, temp8_), {MicroOp::OUTD}, Output(bc_, temp8_), {MicroOp::OUT_R}, InternalOperation(10)),
		NOP, NOP, NOP, NOP,
		NOP_ROW(),	/* 0xc0 */
		NOP_ROW(),	/* 0xd0 */
		NOP_ROW(),	/* 0xe0 */
		NOP_ROW(),	/* 0xf0 */
	};
	assemble_page(target, ed_program_table, false);
#undef NOP_ROW
}

void ProcessorStorage::assemble_cb_page(InstructionPage &target, RegisterPair16 &index, bool add_offsets) {
#define OCTO_OP_GROUP(m, x)	m(x),	m(x),	m(x),	m(x),	m(x),	m(x),	m(x),	m(x)
#define CB_PAGE(m, p)	m(RLC), m(RRC),	m(RL),	m(RR),	m(SLA),	m(SRA),	m(SLL),	m(SRL),	OCTO_OP_GROUP(p, BIT),	OCTO_OP_GROUP(m, RES),	OCTO_OP_GROUP(m, SET)

	InstructionTable cb_program_table = {
		/* 0x00 RLC B;	0x01 RLC C;	0x02 RLC D;	0x03 RLC E;	0x04 RLC H;	0x05 RLC L;	0x06 RLC (HL);	0x07 RLC A */
		/* 0x08 RRC B;	0x09 RRC C;	0x0a RRC D;	0x0b RRC E;	0x0c RRC H;	0x0d RRC L;	0x0e RRC (HL);	0x0f RRC A */
		/* 0x10 RL B;	0x11 RL C;	0x12 RL D;	0x13 RL E;	0x14 RL H;	0x15 RL L;	0x16 RL (HL);	0x17 RL A */
		/* 0x18 RR B;	0x99 RR C;	0x1a RR D;	0x1b RR E;	0x1c RR H;	0x1d RR L;	0x1e RR (HL);	0x1f RR A */
		/* 0x20 SLA B;	0x21 SLA C;	0x22 SLA D;	0x23 SLA E;	0x24 SLA H;	0x25 SLA L;	0x26 SLA (HL);	0x27 SLA A */
		/* 0x28 SRA B;	0x29 SRA C;	0x2a SRA D;	0x2b SRA E;	0x2c SRA H;	0x2d SRA L;	0x2e SRA (HL);	0x2f SRA A */
		/* 0x30 SLL B;	0x31 SLL C;	0x32 SLL D;	0x33 SLL E;	0x34 SLL H;	0x35 SLL L;	0x36 SLL (HL);	0x37 SLL A */
		/* 0x38 SRL B;	0x39 SRL C;	0x3a SRL D;	0x3b SRL E;	0x3c SRL H;	0x3d SRL L;	0x3e SRL (HL);	0x3f SRL A */
		/* 0x40 - 0x7f: BIT */
		/* 0x80 - 0xcf: RES */
		/* 0xd0 - 0xdf: SET */
		CB_PAGE(MODIFY_OP_GROUP, READ_OP_GROUP_D)
	};
	InstructionTable offsets_cb_program_table = {
		CB_PAGE(IX_MODIFY_OP_GROUP, IX_READ_OP_GROUP)
	};
	assemble_page(target, add_offsets ? offsets_cb_program_table : cb_program_table, add_offsets);

#undef OCTO_OP_GROUP
#undef CB_PAGE
}

void ProcessorStorage::assemble_base_page(InstructionPage &target, RegisterPair16 &index, bool add_offsets, InstructionPage &cb_page) {
#define INC_DEC_LD(r)	\
				Sequence({MicroOp::Increment8, &r}),	\
				Sequence({MicroOp::Decrement8, &r}),	\
				Sequence(ReadInc(pc_, r))

#define INC_INC_DEC_LD(rf, r)	\
				Sequence(InternalOperation(4), {MicroOp::Increment16, &rf.full}), INC_DEC_LD(r)

#define DEC_INC_DEC_LD(rf, r)	\
				Sequence(InternalOperation(4), {MicroOp::Decrement16, &rf.full}), INC_DEC_LD(r)

	InstructionTable base_program_table = {
		/* 0x00 NOP */			NOP,								/* 0x01 LD BC, nn */	Sequence(Read16Inc(pc_, bc_)),
		/* 0x02 LD (BC), A */	Sequence({MicroOp::SetAddrAMemptr, &bc_.full}, Write3(bc_, a_)),

		/* 0x03 INC BC;	0x04 INC B;	0x05 DEC B;	0x06 LD B, n */
		INC_INC_DEC_LD(bc_, bc_.halves.high),

		/* 0x07 RLCA */			Sequence({MicroOp::RLCA}),
		/* 0x08 EX AF, AF' */	Sequence({MicroOp::ExAFAFDash}),	/* 0x09 ADD HL, BC */	ADD16(index, bc_),
		/* 0x0a LD A, (BC) */	Sequence({MicroOp::Move16, &bc_.full, &memptr_.full}, Read3(memptr_, a_), Inc16(memptr_)),

		/* 0x0b DEC BC;	0x0c INC C; 0x0d DEC C; 0x0e LD C, n */
		DEC_INC_DEC_LD(bc_, bc_.halves.low),

		/* 0x0f RRCA */			Sequence({MicroOp::RRCA}),
		/* 0x10 DJNZ */			Sequence(InternalOperation(2), ReadInc(pc_, temp8_), {MicroOp::DJNZ}, InternalOperation(10), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &memptr_.full, &pc_.full}),
		/* 0x11 LD DE, nn */	Sequence(Read16Inc(pc_, de_)),
		/* 0x12 LD (DE), A */	Sequence({MicroOp::SetAddrAMemptr, &de_.full}, Write3(de_, a_)),

		/* 0x13 INC DE;	0x14 INC D;	0x15 DEC D;	0x16 LD D, n */
		INC_INC_DEC_LD(de_, de_.halves.high),

		/* 0x17 RLA */			Sequence({MicroOp::RLA}),
		/* 0x18 JR */			Sequence(ReadInc(pc_, temp8_), InternalOperation(10), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &memptr_.full, &pc_.full}),
		/* 0x19 ADD HL, DE */	ADD16(index, de_),
		/* 0x1a LD A, (DE) */	Sequence({MicroOp::Move16, &de_.full, &memptr_.full}, Read3(memptr_, a_), Inc16(memptr_)),

		/* 0x1b DEC DE;	0x1c INC E; 0x1d DEC E; 0x1e LD E, n */
		DEC_INC_DEC_LD(de_, de_.halves.low),

		/* 0x1f RRA */			Sequence({MicroOp::RRA}),
		/* 0x20 JR NZ */		JR(TestNZ),							 /* 0x21 LD HL, nn */	Sequence(Read16Inc(pc_, index)),
		/* 0x22 LD (nn), HL */	Sequence(Read16Inc(pc_, memptr_), Write16(memptr_, index)),

		/* 0x23 INC HL;	0x24 INC H;	0x25 DEC H;	0x26 LD H, n */
		INC_INC_DEC_LD(index, index.halves.high),

		/* 0x27 DAA */			Sequence({MicroOp::DAA}),
		/* 0x28 JR Z */			JR(TestZ),							/* 0x29 ADD HL, HL */	ADD16(index, index),
		/* 0x2a LD HL, (nn) */	Sequence(Read16Inc(pc_, memptr_), Read16(memptr_, index)),

		/* 0x2b DEC HL;	0x2c INC L; 0x2d DEC L; 0x2e LD L, n */
		DEC_INC_DEC_LD(index, index.halves.low),

		/* 0x2f CPL */			Sequence({MicroOp::CPL}),
		/* 0x30 JR NC */		JR(TestNC),							/* 0x31 LD SP, nn */	Sequence(Read16Inc(pc_, sp_)),
		/* 0x32 LD (nn), A */	Sequence(Read16Inc(pc_, temp16_), {MicroOp::SetAddrAMemptr, &temp16_.full}, Write3(temp16_, a_)),
		/* 0x33 INC SP */		Sequence(InternalOperation(4), {MicroOp::Increment16, &sp_.full}),
		/* 0x34 INC (HL) */		Sequence(INDEX(), Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::Increment8, &temp8_}, Write3(INDEX_ADDR(), temp8_)),
		/* 0x35 DEC (HL) */		Sequence(INDEX(), Read4Pre(INDEX_ADDR(), temp8_), {MicroOp::Decrement8, &temp8_}, Write3(INDEX_ADDR(), temp8_)),
		/* 0x36 LD (HL), n */	Sequence(ReadInc(pc_, temp8_), Write3(INDEX_ADDR(), temp8_)),
		/* 0x37 SCF */			Sequence({MicroOp::SCF}),
		/* 0x38 JR C */			JR(TestC),
		/* 0x39 ADD HL, SP */	ADD16(index, sp_),
		/* 0x3a LD A, (nn) */	Sequence(Read16Inc(pc_, memptr_), Read3(memptr_, a_), Inc16(memptr_)),
		/* 0x3b DEC SP */		Sequence(InternalOperation(4), {MicroOp::Decrement16, &sp_.full}),

		/* 0x3c INC A;	0x3d DEC A;	0x3e LD A, n */
		INC_DEC_LD(a_),

		/* 0x3f CCF */			Sequence({MicroOp::CCF}),

		/* 0x40 LD B, B;  0x41 LD B, C;	0x42 LD B, D;	0x43 LD B, E;	0x44 LD B, H;	0x45 LD B, L;	0x46 LD B, (HL);	0x47 LD B, A */
		LD_GROUP(bc_.halves.high, bc_.halves.high),

		/* 0x48 LD C, B;  0x49 LD C, C;	0x4a LD C, D;	0x4b LD C, E;	0x4c LD C, H;	0x4d LD C, L;	0x4e LD C, (HL);	0x4f LD C, A */
		LD_GROUP(bc_.halves.low, bc_.halves.low),

		/* 0x50 LD D, B;  0x51 LD D, C;	0x52 LD D, D;	0x53 LD D, E;	0x54 LD D, H;	0x55 LD D, L;	0x56 LD D, (HL);	0x57 LD D, A */
		LD_GROUP(de_.halves.high, de_.halves.high),

		/* 0x58 LD E, B;  0x59 LD E, C;	0x5a LD E, D;	0x5b LD E, E;	0x5c LD E, H;	0x5d LD E, L;	0x5e LD E, (HL);	0x5f LD E, A */
		LD_GROUP(de_.halves.low, de_.halves.low),

		/* 0x60 LD H, B;  0x61 LD H, C;	0x62 LD H, D;	0x63 LD H, E;	0x64 LD H, H;	0x65 LD H, L;	0x66 LD H, (HL);	0x67 LD H, A */
		LD_GROUP(index.halves.high, hl_.halves.high),

		/* 0x68 LD L, B;  0x69 LD L, C;	0x6a LD L, D;	0x6b LD L, E;	0x6c LD L, H;	0x6d LD H, L;	0x6e LD L, (HL);	0x6f LD L, A */
		LD_GROUP(index.halves.low, hl_.halves.low),

		/* 0x70 LD (HL), B */	Sequence(INDEX(), Write3(INDEX_ADDR(), bc_.halves.high)),
		/* 0x71 LD (HL), C */	Sequence(INDEX(), Write3(INDEX_ADDR(), bc_.halves.low)),
		/* 0x72 LD (HL), D */	Sequence(INDEX(), Write3(INDEX_ADDR(), de_.halves.high)),
		/* 0x73 LD (HL), E */	Sequence(INDEX(), Write3(INDEX_ADDR(), de_.halves.low)),
		/* 0x74 LD (HL), H */	Sequence(INDEX(), Write3(INDEX_ADDR(), hl_.halves.high)),	// neither of these stores parts of the index register;
		/* 0x75 LD (HL), L */	Sequence(INDEX(), Write3(INDEX_ADDR(), hl_.halves.low)),	// they always store exactly H and L.
		/* 0x76 HALT */			Sequence({MicroOp::HALT}),
		/* 0x77 LD (HL), A */	Sequence(INDEX(), Write3(INDEX_ADDR(), a_)),

		/* 0x78 LD A, B;  0x79 LD A, C;	0x7a LD A, D;	0x7b LD A, E;	0x7c LD A, H;	0x7d LD A, L;	0x7e LD A, (HL);	0x7f LD A, A */
		LD_GROUP(a_, a_),

		/* 0x80 ADD B;	0x81 ADD C;	0x82 ADD D;	0x83 ADD E;	0x84 ADD H;	0x85 ADD L;	0x86 ADD (HL);	0x87 ADD A */
		READ_OP_GROUP(ADD8),

		/* 0x88 ADC B;	0x89 ADC C;	0x8a ADC D;	0x8b ADC E;	0x8c ADC H;	0x8d ADC L;	0x8e ADC (HL);	0x8f ADC A */
		READ_OP_GROUP(ADC8),

		/* 0x90 SUB B;	0x91 SUB C;	0x92 SUB D;	0x93 SUB E;	0x94 SUB H;	0x95 SUB L;	0x96 SUB (HL);	0x97 SUB A */
		READ_OP_GROUP(SUB8),

		/* 0x98 SBC B;	0x99 SBC C;	0x9a SBC D;	0x9b SBC E;	0x9c SBC H;	0x9d SBC L;	0x9e SBC (HL);	0x9f SBC A */
		READ_OP_GROUP(SBC8),

		/* 0xa0 AND B;	0xa1 AND C;	0xa2 AND D;	0xa3 AND E;	0xa4 AND H;	0xa5 AND L;	0xa6 AND (HL);	0xa7 AND A */
		READ_OP_GROUP(And),

		/* 0xa8 XOR B;	0xa9 XOR C;	0xaa XOR D;	0xab XOR E;	0xac XOR H;	0xad XOR L;	0xae XOR (HL);	0xaf XOR A */
		READ_OP_GROUP(Xor),

		/* 0xb0 OR B;	0xb1 OR C;	0xb2 OR D;	0xb3 OR E;	0xb4 OR H;	0xb5 OR L;	0xb6 OR (HL);	0xb7 OR A */
		READ_OP_GROUP(Or),

		/* 0xb8 CP B;	0xb9 CP C;	0xba CP D;	0xbb CP E;	0xbc CP H;	0xbd CP L;	0xbe CP (HL);	0xbf CP A */
		READ_OP_GROUP(CP8),

		/* 0xc0 RET NZ */	RET(TestNZ),							/* 0xc1 POP BC */	Sequence(Pop(bc_)),
		/* 0xc2 JP NZ */	JP(TestNZ),								/* 0xc3 JP nn */	Sequence(Read16(pc_, memptr_), {MicroOp::Move16, &memptr_.full, &pc_.full}),
		/* 0xc4 CALL NZ */	CALL(TestNZ),							/* 0xc5 PUSH BC */	Sequence(InternalOperation(2), Push(bc_)),
		/* 0xc6 ADD A, n */	Sequence(ReadInc(pc_, temp8_), {MicroOp::ADD8, &temp8_}),
		/* 0xc7 RST 00h */	RST(),
		/* 0xc8 RET Z */	RET(TestZ),								/* 0xc9 RET */		Sequence(Pop(memptr_), {MicroOp::Move16, &memptr_.full, &pc_.full}),
		/* 0xca JP Z */		JP(TestZ),								/* 0xcb [CB page] */Sequence(FINDEX(), {MicroOp::SetInstructionPage, &cb_page}),
		/* 0xcc CALL Z */	CALL(TestZ),							/* 0xcd CALL */		Sequence(ReadInc(pc_, memptr_.halves.low), Read4Inc(pc_, memptr_.halves.high), Push(pc_), {MicroOp::Move16, &memptr_.full, &pc_.full}),
		/* 0xce ADC A, n */	Sequence(ReadInc(pc_, temp8_), {MicroOp::ADC8, &temp8_}),
		/* 0xcf RST 08h */	RST(),
		/* 0xd0 RET NC */	RET(TestNC),							/* 0xd1 POP DE */	Sequence(Pop(de_)),
		/* 0xd2 JP NC */	JP(TestNC),								/* 0xd3 OUT (n), A */Sequence(ReadInc(pc_, memptr_.halves.low), {MicroOp::Move8, &a_, &memptr_.halves.high}, Output(memptr_, a_), Inc8NoFlags(memptr_.halves.low)),
		/* 0xd4 CALL NC */	CALL(TestNC),							/* 0xd5 PUSH DE */	Sequence(InternalOperation(2), Push(de_)),
		/* 0xd6 SUB n */	Sequence(ReadInc(pc_, temp8_), {MicroOp::SUB8, &temp8_}),
		/* 0xd7 RST 10h */	RST(),
		/* 0xd8 RET C */	RET(TestC),								/* 0xd9 EXX */		Sequence({MicroOp::EXX}),
		/* 0xda JP C */		JP(TestC),								/* 0xdb IN A, (n) */Sequence(ReadInc(pc_, memptr_.halves.low), {MicroOp::Move8, &a_, &memptr_.halves.high}, Input(memptr_, a_), Inc16(memptr_)),
		/* 0xdc CALL C */	CALL(TestC),							/* 0xdd [DD page] */Sequence({MicroOp::SetInstructionPage, &dd_page_}),
		/* 0xde SBC A, n */	Sequence(ReadInc(pc_, temp8_), {MicroOp::SBC8, &temp8_}),
		/* 0xdf RST 18h */	RST(),
		/* 0xe0 RET PO */	RET(TestPO),							/* 0xe1 POP HL */	Sequence(Pop(index)),
		/* 0xe2 JP PO */	JP(TestPO),								/* 0xe3 EX (SP), HL */Sequence(Pop7(memptr_), Push8(index), {MicroOp::Move16, &memptr_.full, &index.full}),
		/* 0xe4 CALL PO */	CALL(TestPO),							/* 0xe5 PUSH HL */	Sequence(InternalOperation(2), Push(index)),
		/* 0xe6 AND n */	Sequence(ReadInc(pc_, temp8_), {MicroOp::And, &temp8_}),
		/* 0xe7 RST 20h */	RST(),
		/* 0xe8 RET PE */	RET(TestPE),							/* 0xe9 JP (HL) */	Sequence({MicroOp::Move16, &index.full, &pc_.full}),
		/* 0xea JP PE */	JP(TestPE),								/* 0xeb EX DE, HL */Sequence({MicroOp::ExDEHL}),
		/* 0xec CALL PE */	CALL(TestPE),							/* 0xed [ED page] */Sequence({MicroOp::SetInstructionPage, &ed_page_}),
		/* 0xee XOR n */	Sequence(ReadInc(pc_, temp8_), {MicroOp::Xor, &temp8_}),
		/* 0xef RST 28h */	RST(),
		/* 0xf0 RET p */	RET(TestP),								/* 0xf1 POP AF */	Sequence(Pop(temp16_), {MicroOp::DisassembleAF}),
		/* 0xf2 JP P */		JP(TestP),								/* 0xf3 DI */		Sequence({MicroOp::DI}),
		/* 0xf4 CALL P */	CALL(TestP),							/* 0xf5 PUSH AF */	Sequence(InternalOperation(2), {MicroOp::AssembleAF}, Push(temp16_)),
		/* 0xf6 OR n */		Sequence(ReadInc(pc_, temp8_), {MicroOp::Or, &temp8_}),
		/* 0xf7 RST 30h */	RST(),
		/* 0xf8 RET M */	RET(TestM),								/* 0xf9 LD SP, HL */Sequence(InternalOperation(4), {MicroOp::Move16, &index.full, &sp_.full}),
		/* 0xfa JP M */		JP(TestM),								/* 0xfb EI */		Sequence({MicroOp::EI}),
		/* 0xfc CALL M */	CALL(TestM),							/* 0xfd [FD page] */Sequence({MicroOp::SetInstructionPage, &fd_page_}),
		/* 0xfe CP n */		Sequence(ReadInc(pc_, temp8_), {MicroOp::CP8, &temp8_}),
		/* 0xff RST 38h */	RST(),
	};

	if(add_offsets) {
		// The indexed version of 0x36 differs substantially from the non-indexed by building index calculation into
		// the cycle that fetches the final operand. So patch in a different microprogram if building an indexed table.
		InstructionTable copy_table = {
			Sequence(FINDEX(), Read5Inc(pc_, temp8_), Write3(INDEX_ADDR(), temp8_))
		};
		std::memcpy(&base_program_table[0x36], &copy_table[0], sizeof(copy_table[0]));
	}

	assemble_cb_page(cb_page, index, add_offsets);
	assemble_page(target, base_program_table, add_offsets);
}

void ProcessorStorage::assemble_fetch_decode_execute(InstructionPage &target, int length) {
	/// The fetch-decode-execute sequence for a regular four-clock M1 cycle.
	const MicroOp normal_fetch_decode_execute[] = {
		BusOp(ReadOpcodeStart()),
		BusOp(ReadOpcodeWait(true)),
		BusOp(ReadOpcodeEnd()),
		{ MicroOp::IncrementR },
		BusOp(Refresh()),
		{ MicroOp::DecodeOperation }
	};

	/// The concluding fetch-decode-execute of a [dd/fd]cb nn oo sequence, i.e. an (IX+n) or (IY+n) operation.
	/// Per the observed 48kb/128kb Spectrum timings, this appears not to include a refresh cycle. So I've also
	/// taken a punt on it not incrementing R.
	const MicroOp short_fetch_decode_execute[] = {
		BusOp(ReadStart(pc_, operation_)),
		BusOp(ReadWait(2, pc_, operation_, true)),
		BusOp(ReadEnd(pc_, operation_)),
		InternalOperation(4),
		{ MicroOp::DecodeOperation },
	};

	copy_program((length == 4) ? normal_fetch_decode_execute : short_fetch_decode_execute, target.fetch_decode_execute);
	target.fetch_decode_execute_data = target.fetch_decode_execute.data();
}

bool ProcessorBase::is_starting_new_instruction() const {
	return
		current_instruction_page_ == &base_page_ &&
		scheduled_program_counter_ == &base_page_.fetch_decode_execute[0];
}

bool ProcessorBase::get_is_resetting() const {
	return request_status_ & (Interrupt::PowerOn | Interrupt::Reset);
}
