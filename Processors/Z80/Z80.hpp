//
//  Z80.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Z80_hpp
#define Z80_hpp

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>

#include "../RegisterSizes.hpp"

namespace CPU {
namespace Z80 {

/*
	The list of registers that can be accessed via @c set_value_of_register and @c set_value_of_register.
*/
enum Register {
	ProgramCounter,
	StackPointer,

	A,		Flags,	AF,
	B,		C,		BC,
	D,		E,		DE,
	H,		L,		HL,

	ADash,		FlagsDash,	AFDash,
	BDash,		CDash,		BCDash,
	DDash,		EDash,		DEDash,
	HDash,		LDash,		HLDash,

	IXh,	IXl,	IX,
	IYh,	IYl,	IY,
	R,		I,		Refresh,

	IFF1,	IFF2,	IM
};

/*
	Flags as defined on the Z80; can be used to decode the result of @c get_flags or to form a value for @c set_flags.
*/
enum Flag: uint8_t {
	Sign		= 0x80,
	Zero		= 0x40,
	Bit5		= 0x20,
	HalfCarry	= 0x10,
	Bit3		= 0x08,
	Parity		= 0x04,
	Overflow	= 0x04,
	Subtract	= 0x02,
	Carry		= 0x01
};

/*!
	Subclasses will be given the task of performing bus operations, allowing them to provide whatever interface they like
	between a Z80 and the rest of the system. @c BusOperation lists the types of bus operation that may be requested.
*/
struct MachineCycle {
	enum Operation {
		ReadOpcodeStart = 0,
		ReadOpcodeWait,
		Read,
		Write,
		Input,
		Output,
		Interrupt,

		Refresh,
		Internal,
		BusAcknowledge,

		ReadStart,
		ReadWait,
		WriteStart,
		WriteWait,
		InputStart,
		InputWait,
		OutputStart,
		OutputWait
	} operation;
	int length;
	uint16_t *address;
	uint8_t *value;
	bool was_requested;

	inline bool expects_action() const {
		return operation <= Operation::Interrupt;
	}
	inline bool is_terminal() const {
		return operation <= Operation::BusAcknowledge;
	}
};

// Elemental bus operations
#define ReadOpcodeStart()			{MachineCycle::ReadOpcodeStart, 2, &pc_.full, &operation_, false}
#define ReadOpcodeWait(length, f)	{MachineCycle::ReadOpcodeWait, length, &pc_.full, &operation_, f}
#define Refresh(len)				{MachineCycle::Refresh, len, &ir_.full, nullptr, false}

#define ReadStart(addr, val)		{MachineCycle::ReadStart, 2, &addr.full, &val, false}
#define ReadWait(l, addr, val, f)	{MachineCycle::ReadWait, l, &addr.full, &val, f}
#define ReadEnd(addr, val)			{MachineCycle::Read, 1, &addr.full, &val, false}

#define WriteStart(addr, val)		{MachineCycle::WriteStart, 2, &addr.full, &val, false}
#define WriteWait(l, addr, val, f)	{MachineCycle::WriteWait, l, &addr.full, &val, f}
#define WriteEnd(addr, val)			{MachineCycle::Write, 1, &addr.full, &val, false}

#define InputStart(addr, val)		{MachineCycle::InputStart, 2, &addr.full, &val, false}
#define InputWait(addr, val, f)		{MachineCycle::InputWait, 1, &addr.full, &val, f}
#define InputEnd(addr, val)			{MachineCycle::Input, 1, &addr.full, &val, false}

#define OutputStart(addr, val)		{MachineCycle::OutputStart, 2, &addr.full, &val}
#define OutputWait(addr, val, f)	{MachineCycle::OutputWait, 1, &addr.full, &val, f}
#define OutputEnd(addr, val)		{MachineCycle::Output, 1, &addr.full, &val}

#define IntAck(length, val)			{MachineCycle::Interrupt, length, nullptr, &val}

// A wrapper to express a bus operation as a micro-op
#define BusOp(op)					{MicroOp::BusOperation, nullptr, nullptr, op}

// Compound bus operations, as micro-ops
#define Read3(addr, val)			BusOp(ReadStart(addr, val)), BusOp(ReadWait(1, addr, val, true)), BusOp(ReadEnd(addr, val))
#define Read4(addr, val)			BusOp(ReadStart(addr, val)), BusOp(ReadWait(1, addr, val, false)), BusOp(ReadWait(1, addr, val, true)), BusOp(ReadEnd(addr, val))
#define Read5(addr, val)			BusOp(ReadStart(addr, val)), BusOp(ReadWait(2, addr, val, false)), BusOp(ReadWait(1, addr, val, true)), BusOp(ReadEnd(addr, val))

#define Write3(addr, val)			BusOp(WriteStart(addr, val)), BusOp(WriteWait(1, addr, val, true)), BusOp(WriteEnd(addr, val))
#define Write5(addr, val)			BusOp(WriteStart(addr, val)), BusOp(WriteWait(2, addr, val, false)), BusOp(WriteWait(1, addr, val, true)), BusOp(WriteEnd(addr, val))

#define Input(addr, val)			BusOp(InputStart(addr, val)), BusOp(InputWait(addr, val, false)), BusOp(InputWait(addr, val, true)), BusOp(InputEnd(addr, val))
#define Output(addr, val)			BusOp(OutputStart(addr, val)), BusOp(OutputWait(addr, val, false)), BusOp(OutputWait(addr, val, true)), BusOp(OutputEnd(addr, val))
#define InternalOperation(len)		{MicroOp::BusOperation, nullptr, nullptr, {MachineCycle::Internal, len}}

/// A sequence is a series of micro-ops that ends in a move-to-next-program operation.
#define Sequence(...)				{ __VA_ARGS__, {MicroOp::MoveToNextProgram} }

/// An instruction is the part of an instruction that follows instruction fetch; it should include two or more refresh cycles and then the work of the instruction.
#define Instr(r, ...)				Sequence(BusOp(Refresh(r)), __VA_ARGS__)

/// A standard instruction is one with the most normal timing: two cycles of refresh, then the work.
#define StdInstr(...)				Instr(2, __VA_ARGS__)

// Assumption made: those instructions that are rated with an opcode fetch greater than four cycles spend the extra time
// providing a lengthened refresh cycle. I assume this because the CPU doesn't have foresight and presumably spends the
// normal refresh time decoding. So if it gets to cycle four and realises it has two more cycles of work, I have assumed
// it simply maintains the refresh state for an extra two cycles.

/*!
	@abstact An abstract base class for emulation of a Z80 processor via the curiously recurring template pattern/f-bounded polymorphism.

	@discussion Subclasses should implement @c perform_machine_cycle in
	order to provide the bus on which the Z80 operates and @c flush(), which is called upon completion of a continuous run
	of cycles to allow a subclass to bring any on-demand activities up to date.
*/
template <class T> class Processor {
	private:
		uint8_t a_;
		RegisterPair bc_, de_, hl_;
		RegisterPair afDash_, bcDash_, deDash_, hlDash_;
		RegisterPair ix_, iy_, pc_, sp_;
		RegisterPair ir_;
		bool iff1_, iff2_;
		int interrupt_mode_;
		uint16_t pc_increment_;
		uint8_t sign_result_;				// the sign flag is set if the value in sign_result_ is negative
		uint8_t zero_result_;				// the zero flag is set if the value in zero_result_ is zero
		uint8_t half_carry_result_;			// the half-carry flag is set if bit 4 of half_carry_result_ is set
		uint8_t bit53_result_;				// the bit 3 and 5 flags are set if the corresponding bits of bit53_result_ are set
		uint8_t parity_overflow_result_;	// the parity/overflow flag is set if the corresponding bit of parity_overflow_result_ is set
		uint8_t subtract_flag_;				// contains a copy of the subtract flag in isolation
		uint8_t carry_result_;				// the carry flag is set if bit 0 of carry_result_ is set
		uint8_t halt_mask_;

		int number_of_cycles_;

		enum Interrupt: uint8_t {
			IRQ			= 0x01,
			NMI			= 0x02,
			Reset		= 0x04,
			PowerOn		= 0x08
		};
		uint8_t request_status_;
		uint8_t last_request_status_;
		bool irq_line_;
		bool bus_request_line_;
		bool wait_line_;

		uint8_t operation_;
		RegisterPair temp16_, memptr_;
		uint8_t temp8_;

		struct MicroOp {
			enum Type {
				BusOperation,
				DecodeOperation,
				DecodeOperationNoRChange,
				MoveToNextProgram,

				Increment8,
				Increment16,
				Decrement8,
				Decrement16,
				Move8,
				Move16,

				IncrementPC,

				AssembleAF,
				DisassembleAF,

				And,
				Or,
				Xor,

				TestNZ,
				TestZ,
				TestNC,
				TestC,
				TestPO,
				TestPE,
				TestP,
				TestM,

				ADD16,	ADC16,	SBC16,
				CP8,	SUB8,	SBC8,	ADD8,	ADC8,
				NEG,

				ExDEHL, ExAFAFDash, EXX,

				EI,		DI,		IM,

				LDI,	LDIR,	LDD,	LDDR,
				CPI,	CPIR,	CPD,	CPDR,
				INI,	INIR,	IND,	INDR,
				OUTI,	OUTD,	OUT_R,

				RLA,	RLCA,	RRA,	RRCA,
				RLC,	RRC,	RL,		RR,
				SLA,	SRA,	SLL,	SRL,
				RLD,	RRD,

				SetInstructionPage,
				CalculateIndexAddress,

				BeginNMI,
				BeginIRQ,
				BeginIRQMode0,
				RETN,
				JumpTo66,
				HALT,

				DJNZ,
				DAA,
				CPL,
				SCF,
				CCF,

				RES,
				BIT,
				SET,

				CalculateRSTDestination,

				SetAFlags,
				SetInFlags,
				SetZero,

				IndexedPlaceHolder,

				Reset
			};
			Type type;
			void *source;
			void *destination;
			MachineCycle machine_cycle;
		};
		const MicroOp *scheduled_program_counter_;


		struct InstructionPage {
			std::vector<MicroOp *> instructions;
			std::vector<MicroOp> all_operations;
			std::vector<MicroOp> fetch_decode_execute;
			MicroOp *fetch_decode_execute_data;
			uint8_t r_step;
			bool is_indexed;

			InstructionPage() : r_step(1), is_indexed(false) {}
		};
		std::vector<MicroOp> conditional_call_untaken_program_;
		std::vector<MicroOp> reset_program_;
		std::vector<MicroOp> irq_program_[3];
		std::vector<MicroOp> nmi_program_;
		InstructionPage *current_instruction_page_;

		InstructionPage base_page_;
		InstructionPage ed_page_;
		InstructionPage fd_page_;
		InstructionPage dd_page_;

		InstructionPage cb_page_;
		InstructionPage fdcb_page_;
		InstructionPage ddcb_page_;


/* The following are helper macros that define common parts of instructions */
#define Inc16(r)				{(&r == &pc_) ? MicroOp::IncrementPC : MicroOp::Increment16, &r.full}

#define ReadInc(addr, val)		Read3(addr, val), Inc16(addr)
#define Read4Inc(addr, val)		Read4(addr, val), Inc16(addr)
#define Read5Inc(addr, val)		Read5(addr, val), Inc16(addr)
#define WriteInc(addr, val)		Write3(addr, val), {MicroOp::Increment16, &addr.full}

#define Read16Inc(addr, val)	ReadInc(addr, val.bytes.low), ReadInc(addr, val.bytes.high)
#define Read16(addr, val)		ReadInc(addr, val.bytes.low), Read3(addr, val.bytes.high)

#define Write16(addr, val)		WriteInc(addr, val.bytes.low), Write3(addr, val.bytes.high)

#define INDEX()					{MicroOp::IndexedPlaceHolder}, ReadInc(pc_, temp8_), InternalOperation(5), {MicroOp::CalculateIndexAddress, &index}
#define FINDEX()				{MicroOp::IndexedPlaceHolder}, ReadInc(pc_, temp8_), {MicroOp::CalculateIndexAddress, &index}
#define INDEX_ADDR()			(add_offsets ? memptr_ : index)

#define Push(x)					{MicroOp::Decrement16, &sp_.full}, Write3(sp_, x.bytes.high), {MicroOp::Decrement16, &sp_.full}, Write3(sp_, x.bytes.low)
#define Pop(x)					Read3(sp_, x.bytes.low), {MicroOp::Increment16, &sp_.full}, Read3(sp_, x.bytes.high), {MicroOp::Increment16, &sp_.full}

#define Push8(x)				{MicroOp::Decrement16, &sp_.full}, Write3(sp_, x.bytes.high), {MicroOp::Decrement16, &sp_.full}, Write5(sp_, x.bytes.low)
#define Pop7(x)					Read3(sp_, x.bytes.low), {MicroOp::Increment16, &sp_.full}, Read4(sp_, x.bytes.high), {MicroOp::Increment16, &sp_.full}

/* The following are actual instructions */
#define NOP						Sequence(BusOp(Refresh(2)))

#define JP(cc)					StdInstr(Read16Inc(pc_, temp16_), {MicroOp::cc, nullptr}, {MicroOp::Move16, &temp16_.full, &pc_.full})
#define CALL(cc)				StdInstr(ReadInc(pc_, temp16_.bytes.low), {MicroOp::cc, conditional_call_untaken_program_.data()}, Read4Inc(pc_, temp16_.bytes.high), Push(pc_), {MicroOp::Move16, &temp16_.full, &pc_.full})
#define RET(cc)					Instr(3, {MicroOp::cc, nullptr}, Pop(memptr_), {MicroOp::Move16, &memptr_.full, &pc_.full})
#define JR(cc)					StdInstr(ReadInc(pc_, temp8_), {MicroOp::cc, nullptr}, InternalOperation(5), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &memptr_.full, &pc_.full})
#define RST()					Instr(3, {MicroOp::CalculateRSTDestination}, Push(pc_), {MicroOp::Move16, &memptr_.full, &pc_.full})
#define LD(a, b)				StdInstr({MicroOp::Move8, &b, &a})

#define LD_GROUP(r, ri)	\
				LD(r, bc_.bytes.high),		LD(r, bc_.bytes.low),	LD(r, de_.bytes.high),						LD(r, de_.bytes.low),	\
				LD(r, index.bytes.high),	LD(r, index.bytes.low),		\
				StdInstr(INDEX(), Read3(INDEX_ADDR(), temp8_), {MicroOp::Move8, &temp8_, &ri}),		\
				LD(r, a_)

#define READ_OP_GROUP(op)	\
				StdInstr({MicroOp::op, &bc_.bytes.high}),	StdInstr({MicroOp::op, &bc_.bytes.low}),	\
				StdInstr({MicroOp::op, &de_.bytes.high}),	StdInstr({MicroOp::op, &de_.bytes.low}),	\
				StdInstr({MicroOp::op, &index.bytes.high}),	StdInstr({MicroOp::op, &index.bytes.low}),	\
				StdInstr(INDEX(), Read3(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr({MicroOp::op, &a_})

#define READ_OP_GROUP_D(op)	\
				StdInstr({MicroOp::op, &bc_.bytes.high}),	StdInstr({MicroOp::op, &bc_.bytes.low}),	\
				StdInstr({MicroOp::op, &de_.bytes.high}),	StdInstr({MicroOp::op, &de_.bytes.low}),	\
				StdInstr({MicroOp::op, &index.bytes.high}),	StdInstr({MicroOp::op, &index.bytes.low}),	\
				StdInstr(INDEX(), Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr({MicroOp::op, &a_})

#define RMW(x, op, ...) StdInstr(INDEX(), Read4(INDEX_ADDR(), x), {MicroOp::op, &x}, Write3(INDEX_ADDR(), x))
#define RMWI(x, op, ...) StdInstr(Read4(INDEX_ADDR(), x), {MicroOp::op, &x}, Write3(INDEX_ADDR(), x))

#define MODIFY_OP_GROUP(op)	\
				StdInstr({MicroOp::op, &bc_.bytes.high}),	StdInstr({MicroOp::op, &bc_.bytes.low}),	\
				StdInstr({MicroOp::op, &de_.bytes.high}),	StdInstr({MicroOp::op, &de_.bytes.low}),	\
				StdInstr({MicroOp::op, &index.bytes.high}),	StdInstr({MicroOp::op, &index.bytes.low}),	\
				RMW(temp8_, op),	\
				StdInstr({MicroOp::op, &a_})

#define IX_MODIFY_OP_GROUP(op)	\
				RMWI(bc_.bytes.high, op),	\
				RMWI(bc_.bytes.low, op),	\
				RMWI(de_.bytes.high, op),	\
				RMWI(de_.bytes.low, op),	\
				RMWI(hl_.bytes.high, op),	\
				RMWI(hl_.bytes.low, op),	\
				RMWI(temp8_, op),	\
				RMWI(a_, op)

#define IX_READ_OP_GROUP(op)	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_}),	\
				StdInstr(Read4(INDEX_ADDR(), temp8_), {MicroOp::op, &temp8_})

#define ADD16(d, s) StdInstr(InternalOperation(4), InternalOperation(3), {MicroOp::ADD16, &s.full, &d.full})
#define ADC16(d, s) StdInstr(InternalOperation(4), InternalOperation(3), {MicroOp::ADC16, &s.full, &d.full})
#define SBC16(d, s) StdInstr(InternalOperation(4), InternalOperation(3), {MicroOp::SBC16, &s.full, &d.full})

#define isTerminal(n)	(n == MicroOp::MoveToNextProgram || n == MicroOp::DecodeOperation || n == MicroOp::DecodeOperationNoRChange)

		typedef MicroOp InstructionTable[256][30];

		void assemble_page(InstructionPage &target, InstructionTable &table, bool add_offsets) {
			size_t number_of_micro_ops = 0;
			size_t lengths[256];

			// Count number of micro-ops required.
			for(int c = 0; c < 256; c++) {
				size_t length = 0;
				while(!isTerminal(table[c][length].type)) length++;
				length++;
				lengths[c] = length;
				number_of_micro_ops += length;
			}

			// Allocate a landing area.
			target.all_operations.resize(number_of_micro_ops);
			target.instructions.resize(256, nullptr);

			// Copy in all programs and set pointers.
			size_t destination = 0;
			for(size_t c = 0; c < 256; c++) {
				target.instructions[c] = &target.all_operations[destination];
				for(int t = 0; t < lengths[c];) {
					// Skip zero-length bus cycles.
					if(table[c][t].type == MicroOp::BusOperation && table[c][t].machine_cycle.length == 0) {
						t++;
						continue;
					}

					// If an index placeholder is hit then drop it, and if offsets aren't being added,
					// then also drop the indexing that follows, which is assumed to be everything
					// up to and including the next ::CalculateIndexAddress. Coupled to the INDEX() macro.
					if(table[c][t].type == MicroOp::IndexedPlaceHolder) {
						t++;
						if(!add_offsets) {
							while(table[c][t].type != MicroOp::CalculateIndexAddress) t++;
							t++;
						}
					}
					target.all_operations[destination] = table[c][t];
					destination++;
					t++;
				}
			}
		}

		void assemble_ed_page(InstructionPage &target) {
#define IN_C(r)		StdInstr(Input(bc_, r), {MicroOp::SetInFlags, &r})
#define OUT_C(r)	StdInstr(Output(bc_, r))
#define IN_OUT(r)	IN_C(r), OUT_C(r)

#define NOP_ROW()	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP
			InstructionTable ed_program_table = {
				NOP_ROW(),	/* 0x00 */
				NOP_ROW(),	/* 0x10 */
				NOP_ROW(),	/* 0x20 */
				NOP_ROW(),	/* 0x30 */
				/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(bc_.bytes.high),
				/* 0x42 SBC HL, BC */	SBC16(hl_, bc_),				/* 0x43 LD (nn), BC */	StdInstr(Read16Inc(pc_, temp16_), Write16(temp16_, bc_)),
				/* 0x44 NEG */			StdInstr({MicroOp::NEG}),		/* 0x45 RETN */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x46 IM 0 */			StdInstr({MicroOp::IM}),		/* 0x47 LD I, A */		Instr(3, {MicroOp::Move8, &a_, &ir_.bytes.high}),
				/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(bc_.bytes.low),
				/* 0x4a ADC HL, BC */	ADC16(hl_, bc_),				/* 0x4b LD BC, (nn) */	StdInstr(Read16Inc(pc_, temp16_), Read16(temp16_, bc_)),
				/* 0x4c NEG */			StdInstr({MicroOp::NEG}),		/* 0x4d RETI */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x4e IM 0/1 */		StdInstr({MicroOp::IM}),		/* 0x4f LD R, A */		Instr(3, {MicroOp::Move8, &a_, &ir_.bytes.low}),
				/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(de_.bytes.high),
				/* 0x52 SBC HL, DE */	SBC16(hl_, de_),				/* 0x53 LD (nn), DE */	StdInstr(Read16Inc(pc_, temp16_), Write16(temp16_, de_)),
				/* 0x54 NEG */			StdInstr({MicroOp::NEG}),		/* 0x55 RETN */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x56 IM 1 */			StdInstr({MicroOp::IM}),		/* 0x57 LD A, I */		Instr(3, {MicroOp::Move8, &ir_.bytes.high, &a_}, {MicroOp::SetAFlags}),
				/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(de_.bytes.low),
				/* 0x5a ADC HL, DE */	ADC16(hl_, de_),				/* 0x5b LD DE, (nn) */	StdInstr(Read16Inc(pc_, temp16_), Read16(temp16_, de_)),
				/* 0x5c NEG */			StdInstr({MicroOp::NEG}),		/* 0x5d RETN */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x5e IM 2 */			StdInstr({MicroOp::IM}),		/* 0x5f LD A, R */		Instr(3, {MicroOp::Move8, &ir_.bytes.low, &a_}, {MicroOp::SetAFlags}),
				/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(hl_.bytes.high),
				/* 0x62 SBC HL, HL */	SBC16(hl_, hl_),				/* 0x63 LD (nn), HL */	StdInstr(Read16Inc(pc_, temp16_), Write16(temp16_, hl_)),
				/* 0x64 NEG */			StdInstr({MicroOp::NEG}),		/* 0x65 RETN */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x66 IM 0 */			StdInstr({MicroOp::IM}),		/* 0x67 RRD */			StdInstr(Read3(hl_, temp8_), InternalOperation(4), {MicroOp::RRD}, Write3(hl_, temp8_)),
				/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(hl_.bytes.low),
				/* 0x6a ADC HL, HL */	ADC16(hl_, hl_),				/* 0x6b LD HL, (nn) */	StdInstr(Read16Inc(pc_, temp16_), Read16(temp16_, hl_)),
				/* 0x6c NEG */			StdInstr({MicroOp::NEG}),		/* 0x6d RETN */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x6e IM 0/1 */		StdInstr({MicroOp::IM}),		/* 0x6f RLD */			StdInstr(Read3(hl_, temp8_), InternalOperation(4), {MicroOp::RLD}, Write3(hl_, temp8_)),
				/* 0x70 IN (C) */		IN_C(temp8_),					/* 0x71 OUT (C), 0 */	StdInstr({MicroOp::SetZero}, Output(bc_, temp8_)),
				/* 0x72 SBC HL, SP */	SBC16(hl_, sp_),				/* 0x73 LD (nn), SP */	StdInstr(Read16Inc(pc_, temp16_), Write16(temp16_, sp_)),
				/* 0x74 NEG */			StdInstr({MicroOp::NEG}),		/* 0x75 RETN */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x76 IM 1 */			StdInstr({MicroOp::IM}),		/* 0x77 XX */			NOP,
				/* 0x40 IN B, (C);	0x41 OUT (C), B */	IN_OUT(a_),
				/* 0x7a ADC HL, SP */	ADC16(hl_, sp_),				/* 0x7b LD SP, (nn) */	StdInstr(Read16Inc(pc_, temp16_), Read16(temp16_, sp_)),
				/* 0x7c NEG */			StdInstr({MicroOp::NEG}),		/* 0x7d RETN */			StdInstr(Pop(pc_), {MicroOp::RETN}),
				/* 0x7e IM 2 */			StdInstr({MicroOp::IM}),		/* 0x7f XX */			NOP,
				NOP_ROW(),	/* 0x80 */
				NOP_ROW(),	/* 0x90 */
				/* 0xa0 LDI */		StdInstr(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDI}),
				/* 0xa1 CPI */		StdInstr(Read3(hl_, temp8_), InternalOperation(5), {MicroOp::CPI}),
				/* 0xa2 INI */		Instr(3, Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::INI}),
				/* 0xa3 OTI */		Instr(3, Read3(hl_, temp8_), {MicroOp::OUTI}, Output(bc_, temp8_)),
				NOP, NOP, NOP, NOP,
				/* 0xa8 LDD */		StdInstr(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDD}),
				/* 0xa9 CPD */		StdInstr(Read3(hl_, temp8_), InternalOperation(5), {MicroOp::CPD}),
				/* 0xaa IND */		Instr(3, Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::IND}),
				/* 0xab OTD */		Instr(3, Read3(hl_, temp8_), {MicroOp::OUTD}, Output(bc_, temp8_)),
				NOP, NOP, NOP, NOP,
				/* 0xb0 LDIR */		StdInstr(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDIR}, InternalOperation(5)),
				/* 0xb1 CPIR */		StdInstr(Read3(hl_, temp8_), InternalOperation(5), {MicroOp::CPIR}, InternalOperation(5)),
				/* 0xb2 INIR */		Instr(3, Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::INIR}, InternalOperation(5)),
				/* 0xb3 OTIR */		Instr(3, Read3(hl_, temp8_), {MicroOp::OUTI}, Output(bc_, temp8_), {MicroOp::OUT_R}, InternalOperation(5)),
				NOP, NOP, NOP, NOP,
				/* 0xb8 LDDR */		StdInstr(Read3(hl_, temp8_), Write5(de_, temp8_), {MicroOp::LDDR}, InternalOperation(5)),
				/* 0xb9 CPDR */		StdInstr(Read3(hl_, temp8_), InternalOperation(5), {MicroOp::CPDR}, InternalOperation(5)),
				/* 0xba INDR */		Instr(3, Input(bc_, temp8_), Write3(hl_, temp8_), {MicroOp::INDR}, InternalOperation(5)),
				/* 0xbb OTDR */		Instr(3, Read3(hl_, temp8_), {MicroOp::OUTD}, Output(bc_, temp8_), {MicroOp::OUT_R}, InternalOperation(5)),
				NOP, NOP, NOP, NOP,
				NOP_ROW(),	/* 0xc0 */
				NOP_ROW(),	/* 0xd0 */
				NOP_ROW(),	/* 0xe0 */
				NOP_ROW(),	/* 0xf0 */
			};
			assemble_page(target, ed_program_table, false);
#undef NOP_ROW
		}

		void assemble_cb_page(InstructionPage &target, RegisterPair &index, bool add_offsets) {
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
				/* 0x40 – 0x7f: BIT */
				/* 0x80 – 0xcf: RES */
				/* 0xd0 – 0xdf: SET */
				CB_PAGE(MODIFY_OP_GROUP, READ_OP_GROUP_D)
			};
			InstructionTable offsets_cb_program_table = {
				CB_PAGE(IX_MODIFY_OP_GROUP, IX_READ_OP_GROUP)
			};
			assemble_page(target, add_offsets ? offsets_cb_program_table : cb_program_table, add_offsets);

#undef OCTO_OP_GROUP
#undef CB_PAGE
		}

		void assemble_base_page(InstructionPage &target, RegisterPair &index, bool add_offsets, InstructionPage &cb_page) {
#define INC_DEC_LD(r)	\
				StdInstr({MicroOp::Increment8, &r}),	\
				StdInstr({MicroOp::Decrement8, &r}),	\
				StdInstr(ReadInc(pc_, r))

#define INC_INC_DEC_LD(rf, r)	\
				Instr(4, {MicroOp::Increment16, &rf.full}), INC_DEC_LD(r)

#define DEC_INC_DEC_LD(rf, r)	\
				Instr(4, {MicroOp::Decrement16, &rf.full}), INC_DEC_LD(r)

			InstructionTable base_program_table = {
				/* 0x00 NOP */			NOP,								/* 0x01 LD BC, nn */	StdInstr(Read16Inc(pc_, bc_)),
				/* 0x02 LD (BC), A */	StdInstr({MicroOp::Move16, &bc_.full, &memptr_.full}, Write3(memptr_, a_)),

				/* 0x03 INC BC;	0x04 INC B;	0x05 DEC B;	0x06 LD B, n */
				INC_INC_DEC_LD(bc_, bc_.bytes.high),

				/* 0x07 RLCA */			StdInstr({MicroOp::RLCA}),
				/* 0x08 EX AF, AF' */	StdInstr({MicroOp::ExAFAFDash}),	/* 0x09 ADD HL, BC */	ADD16(index, bc_),
				/* 0x0a LD A, (BC) */	StdInstr({MicroOp::Move16, &bc_.full, &memptr_.full}, Read3(memptr_, a_)),

				/* 0x0b DEC BC;	0x0c INC C; 0x0d DEC C; 0x0e LD C, n */
				DEC_INC_DEC_LD(bc_, bc_.bytes.low),

				/* 0x0f RRCA */			StdInstr({MicroOp::RRCA}),
				/* 0x10 DJNZ */			Instr(3, ReadInc(pc_, temp8_), {MicroOp::DJNZ}, InternalOperation(5), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &memptr_.full, &pc_.full}),
				/* 0x11 LD DE, nn */	StdInstr(Read16Inc(pc_, de_)),
				/* 0x12 LD (DE), A */	StdInstr({MicroOp::Move16, &de_.full, &memptr_.full}, Write3(memptr_, a_)),

				/* 0x13 INC DE;	0x14 INC D;	0x15 DEC D;	0x16 LD D, n */
				INC_INC_DEC_LD(de_, de_.bytes.high),

				/* 0x17 RLA */			StdInstr({MicroOp::RLA}),
				/* 0x18 JR */			StdInstr(ReadInc(pc_, temp8_), InternalOperation(5), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &memptr_.full, &pc_.full}),
				/* 0x19 ADD HL, DE */	ADD16(index, de_),
				/* 0x1a LD A, (DE) */	StdInstr({MicroOp::Move16, &de_.full, &memptr_.full}, Read3(memptr_, a_)),

				/* 0x1b DEC DE;	0x1c INC E; 0x1d DEC E; 0x1e LD E, n */
				DEC_INC_DEC_LD(de_, de_.bytes.low),

				/* 0x1f RRA */			StdInstr({MicroOp::RRA}),
				/* 0x20 JR NZ */		JR(TestNZ),							 /* 0x21 LD HL, nn */	StdInstr(Read16Inc(pc_, index)),
				/* 0x22 LD (nn), HL */	StdInstr(Read16Inc(pc_, temp16_), Write16(temp16_, index)),

				/* 0x23 INC HL;	0x24 INC H;	0x25 DEC H;	0x26 LD H, n */
				INC_INC_DEC_LD(index, index.bytes.high),

				/* 0x27 DAA */			StdInstr({MicroOp::DAA}),
				/* 0x28 JR Z */			JR(TestZ),							/* 0x29 ADD HL, HL */	ADD16(index, index),
				/* 0x2a LD HL, (nn) */	StdInstr(Read16Inc(pc_, temp16_), Read16(temp16_, index)),

				/* 0x2b DEC HL;	0x2c INC L; 0x2d DEC L; 0x2e LD L, n */
				DEC_INC_DEC_LD(index, index.bytes.low),

				/* 0x2f CPL */			StdInstr({MicroOp::CPL}),
				/* 0x30 JR NC */		JR(TestNC),							/* 0x31 LD SP, nn */	StdInstr(Read16Inc(pc_, sp_)),
				/* 0x32 LD (nn), A */	StdInstr(Read16Inc(pc_, temp16_), Write3(temp16_, a_)),
				/* 0x33 INC SP */		Instr(4, {MicroOp::Increment16, &sp_.full}),
				/* 0x34 INC (HL) */		StdInstr(INDEX(), Read4(INDEX_ADDR(), temp8_), {MicroOp::Increment8, &temp8_}, Write3(INDEX_ADDR(), temp8_)),
				/* 0x35 DEC (HL) */		StdInstr(INDEX(), Read4(INDEX_ADDR(), temp8_), {MicroOp::Decrement8, &temp8_}, Write3(INDEX_ADDR(), temp8_)),
				/* 0x36 LD (HL), n */	StdInstr(ReadInc(pc_, temp8_), Write3(INDEX_ADDR(), temp8_)),
				/* 0x37 SCF */			StdInstr({MicroOp::SCF}),
				/* 0x38 JR C */			JR(TestC),
				/* 0x39 ADD HL, SP */	ADD16(index, sp_),
				/* 0x3a LD A, (nn) */	StdInstr(Read16Inc(pc_, memptr_), Read3(memptr_, a_)),
				/* 0x3b DEC SP */		Instr(4, {MicroOp::Decrement16, &sp_.full}),

				/* 0x3c INC A;	0x3d DEC A;	0x3e LD A, n */
				INC_DEC_LD(a_),

				/* 0x3f CCF */			StdInstr({MicroOp::CCF}),

				/* 0x40 LD B, B;  0x41 LD B, C;	0x42 LD B, D;	0x43 LD B, E;	0x44 LD B, H;	0x45 LD B, L;	0x46 LD B, (HL);	0x47 LD B, A */
				LD_GROUP(bc_.bytes.high, bc_.bytes.high),

				/* 0x48 LD C, B;  0x49 LD C, C;	0x4a LD C, D;	0x4b LD C, E;	0x4c LD C, H;	0x4d LD C, L;	0x4e LD C, (HL);	0x4f LD C, A */
				LD_GROUP(bc_.bytes.low, bc_.bytes.low),

				/* 0x50 LD D, B;  0x51 LD D, C;	0x52 LD D, D;	0x53 LD D, E;	0x54 LD D, H;	0x55 LD D, L;	0x56 LD D, (HL);	0x57 LD D, A */
				LD_GROUP(de_.bytes.high, de_.bytes.high),

				/* 0x58 LD E, B;  0x59 LD E, C;	0x5a LD E, D;	0x5b LD E, E;	0x5c LD E, H;	0x5d LD E, L;	0x5e LD E, (HL);	0x5f LD E, A */
				LD_GROUP(de_.bytes.low, de_.bytes.low),

				/* 0x60 LD H, B;  0x61 LD H, C;	0x62 LD H, D;	0x63 LD H, E;	0x64 LD H, H;	0x65 LD H, L;	0x66 LD H, (HL);	0x67 LD H, A */
				LD_GROUP(index.bytes.high, hl_.bytes.high),

				/* 0x68 LD L, B;  0x69 LD L, C;	0x6a LD L, D;	0x6b LD L, E;	0x6c LD L, H;	0x6d LD H, L;	0x6e LD L, (HL);	0x6f LD L, A */
				LD_GROUP(index.bytes.low, hl_.bytes.low),

				/* 0x70 LD (HL), B */	StdInstr(INDEX(), Write3(INDEX_ADDR(), bc_.bytes.high)),
				/* 0x71 LD (HL), C */	StdInstr(INDEX(), Write3(INDEX_ADDR(), bc_.bytes.low)),
				/* 0x72 LD (HL), D */	StdInstr(INDEX(), Write3(INDEX_ADDR(), de_.bytes.high)),
				/* 0x73 LD (HL), E */	StdInstr(INDEX(), Write3(INDEX_ADDR(), de_.bytes.low)),
				/* 0x74 LD (HL), H */	StdInstr(INDEX(), Write3(INDEX_ADDR(), hl_.bytes.high)),	// neither of these stores parts of the index register;
				/* 0x75 LD (HL), L */	StdInstr(INDEX(), Write3(INDEX_ADDR(), hl_.bytes.low)),		// they always store exactly H and L.
				/* 0x76 HALT */			StdInstr({MicroOp::HALT}),
				/* 0x77 LD (HL), A */	StdInstr(INDEX(), Write3(INDEX_ADDR(), a_)),

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

				/* 0xc0 RET NZ */	RET(TestNZ),							/* 0xc1 POP BC */	StdInstr(Pop(bc_)),
				/* 0xc2 JP NZ */	JP(TestNZ),								/* 0xc3 JP nn */	StdInstr(Read16(pc_, temp16_), {MicroOp::Move16, &temp16_.full, &pc_.full}),
				/* 0xc4 CALL NZ */	CALL(TestNZ),							/* 0xc5 PUSH BC */	Instr(3, Push(bc_)),
				/* 0xc6 ADD A, n */	StdInstr(ReadInc(pc_, temp8_), {MicroOp::ADD8, &temp8_}),
				/* 0xc7 RST 00h */	RST(),
				/* 0xc8 RET Z */	RET(TestZ),								/* 0xc9 RET */		StdInstr(Pop(pc_)),
				/* 0xca JP Z */		JP(TestZ),								/* 0xcb [CB page] */StdInstr(FINDEX(), {MicroOp::SetInstructionPage, &cb_page}),
				/* 0xcc CALL Z */	CALL(TestZ),							/* 0xcd CALL */		StdInstr(ReadInc(pc_, temp16_.bytes.low), Read4Inc(pc_, temp16_.bytes.high), Push(pc_), {MicroOp::Move16, &temp16_.full, &pc_.full}),
				/* 0xce ADC A, n */	StdInstr(ReadInc(pc_, temp8_), {MicroOp::ADC8, &temp8_}),
				/* 0xcf RST 08h */	RST(),
				/* 0xd0 RET NC */	RET(TestNC),							/* 0xd1 POP DE */	StdInstr(Pop(de_)),
				/* 0xd2 JP NC */	JP(TestNC),								/* 0xd3 OUT (n), A */StdInstr(ReadInc(pc_, temp16_.bytes.low), {MicroOp::Move8, &a_, &temp16_.bytes.high}, Output(temp16_, a_)),
				/* 0xd4 CALL NC */	CALL(TestNC),							/* 0xd5 PUSH DE */	Instr(3, Push(de_)),
				/* 0xd6 SUB n */	StdInstr(ReadInc(pc_, temp8_), {MicroOp::SUB8, &temp8_}),
				/* 0xd7 RST 10h */	RST(),
				/* 0xd8 RET C */	RET(TestC),								/* 0xd9 EXX */		StdInstr({MicroOp::EXX}),
				/* 0xda JP C */		JP(TestC),								/* 0xdb IN A, (n) */StdInstr(ReadInc(pc_, temp16_.bytes.low), {MicroOp::Move8, &a_, &temp16_.bytes.high}, Input(temp16_, a_)),
				/* 0xdc CALL C */	CALL(TestC),							/* 0xdd [DD page] */StdInstr({MicroOp::SetInstructionPage, &dd_page_}),
				/* 0xde SBC A, n */	StdInstr(ReadInc(pc_, temp8_), {MicroOp::SBC8, &temp8_}),
				/* 0xdf RST 18h */	RST(),
				/* 0xe0 RET PO */	RET(TestPO),							/* 0xe1 POP HL */	StdInstr(Pop(index)),
				/* 0xe2 JP PO */	JP(TestPO),								/* 0xe3 EX (SP), HL */StdInstr(Pop7(memptr_), Push8(index), {MicroOp::Move16, &memptr_.full, &index.full}),
				/* 0xe4 CALL PO */	CALL(TestPO),							/* 0xe5 PUSH HL */	Instr(3, Push(index)),
				/* 0xe6 AND n */	StdInstr(ReadInc(pc_, temp8_), {MicroOp::And, &temp8_}),
				/* 0xe7 RST 20h */	RST(),
				/* 0xe8 RET PE */	RET(TestPE),							/* 0xe9 JP (HL) */	StdInstr({MicroOp::Move16, &index.full, &pc_.full}),
				/* 0xea JP PE */	JP(TestPE),								/* 0xeb EX DE, HL */StdInstr({MicroOp::ExDEHL}),
				/* 0xec CALL PE */	CALL(TestPE),							/* 0xed [ED page] */StdInstr({MicroOp::SetInstructionPage, &ed_page_}),
				/* 0xee XOR n */	StdInstr(ReadInc(pc_, temp8_), {MicroOp::Xor, &temp8_}),
				/* 0xef RST 28h */	RST(),
				/* 0xf0 RET p */	RET(TestP),								/* 0xf1 POP AF */	StdInstr(Pop(temp16_), {MicroOp::DisassembleAF}),
				/* 0xf2 JP P */		JP(TestP),								/* 0xf3 DI */		StdInstr({MicroOp::DI}),
				/* 0xf4 CALL P */	CALL(TestP),							/* 0xf5 PUSH AF */	Instr(3, {MicroOp::AssembleAF}, Push(temp16_)),
				/* 0xf6 OR n */		StdInstr(ReadInc(pc_, temp8_), {MicroOp::Or, &temp8_}),
				/* 0xf7 RST 30h */	RST(),
				/* 0xf8 RET M */	RET(TestM),								/* 0xf9 LD SP, HL */Instr(4, {MicroOp::Move16, &index.full, &sp_.full}),
				/* 0xfa JP M */		JP(TestM),								/* 0xfb EI */		StdInstr({MicroOp::EI}),
				/* 0xfc CALL M */	CALL(TestM),							/* 0xfd [FD page] */StdInstr({MicroOp::SetInstructionPage, &fd_page_}),
				/* 0xfe CP n */		StdInstr(ReadInc(pc_, temp8_), {MicroOp::CP8, &temp8_}),
				/* 0xff RST 38h */	RST(),
			};

			if(add_offsets) {
				// The indexed version of 0x36 differs substantially from the non-indexed by building index calculation into
				// the cycle that fetches the final operand. So patch in a different microprogram if building an indexed table.
				InstructionTable copy_table = {
					StdInstr(FINDEX(), Read5Inc(pc_, temp8_), Write3(INDEX_ADDR(), temp8_))
				};
				memcpy(&base_program_table[0x36], &copy_table[0], sizeof(copy_table[0]));
			}

			assemble_cb_page(cb_page, index, add_offsets);
			assemble_page(target, base_program_table, add_offsets);
		}

		void assemble_fetch_decode_execute(InstructionPage &target, int length) {
			const MicroOp normal_fetch_decode_execute[] = {
				BusOp(ReadOpcodeStart()),
				BusOp(ReadOpcodeWait(1, true)),
				{ MicroOp::DecodeOperation }
			};
			const MicroOp short_fetch_decode_execute[] = {
				BusOp(ReadOpcodeStart()),
				BusOp(ReadOpcodeWait(1, false)),
				BusOp(ReadOpcodeWait(1, true)),
				{ MicroOp::DecodeOperation }
			};
			copy_program((length == 4) ? normal_fetch_decode_execute : short_fetch_decode_execute, target.fetch_decode_execute);
			target.fetch_decode_execute_data = target.fetch_decode_execute.data();
		}

		void copy_program(const MicroOp *source, std::vector<MicroOp> &destination) {
			size_t length = 0;
			while(!isTerminal(source[length].type)) length++;
			destination.resize(length + 1);
			size_t pointer = 0;
			while(true) {
				destination[pointer] = source[pointer];
				if(isTerminal(source[pointer].type)) break;
				pointer++;
			}
		}

	public:
		Processor() :
			halt_mask_(0xff),
			number_of_cycles_(0),
			interrupt_mode_(0),
			wait_line_(false),
			request_status_(Interrupt::PowerOn),
			last_request_status_(Interrupt::PowerOn),
			irq_line_(false),
			bus_request_line_(false),
			pc_increment_(1),
			scheduled_program_counter_(nullptr) {
			set_flags(0xff);

			MicroOp conditional_call_untaken_program[] = Sequence(ReadInc(pc_, temp16_.bytes.high));
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

			MicroOp reset_program[] = Sequence(InternalOperation(3), {MicroOp::Reset});
			MicroOp nmi_program[] = {
				{ MicroOp::BeginNMI },
				BusOp(ReadOpcodeStart()),
				BusOp(ReadOpcodeWait(1, false)),
				BusOp(Refresh(2)),
				Push(pc_),
				{ MicroOp::JumpTo66, nullptr, nullptr},
				{ MicroOp::MoveToNextProgram }
			};
			MicroOp irq_mode0_program[] = {
				{ MicroOp::BeginIRQMode0 },
				BusOp(IntAck(4, operation_)),
				{ MicroOp::DecodeOperationNoRChange }
			};
			MicroOp irq_mode1_program[] = {
				{ MicroOp::BeginIRQ },
				BusOp(IntAck(5, operation_)),
				BusOp(Refresh(2)),
				Push(pc_),
				{ MicroOp::Move16, &temp16_.full, &pc_.full },
				{ MicroOp::MoveToNextProgram }
			};
			MicroOp irq_mode2_program[] = {
				{ MicroOp::BeginIRQ },
				BusOp(IntAck(5, temp16_.bytes.low)),
				BusOp(Refresh(2)),
				Push(pc_),
				{ MicroOp::Move8, &ir_.bytes.high, &temp16_.bytes.high },
				Read16(temp16_, pc_),
				{ MicroOp::MoveToNextProgram }
			};

			copy_program(reset_program, reset_program_);
			copy_program(nmi_program, nmi_program_);
			copy_program(irq_mode0_program, irq_program_[0]);
			copy_program(irq_mode1_program, irq_program_[1]);
			copy_program(irq_mode2_program, irq_program_[2]);
		}

		/*!
			Runs the Z80 for a supplied number of cycles.

			@discussion Subclasses must implement @c perform_machine_cycle(const MachineCycle &cycle) .

			If it is a read operation then @c value will be seeded with the value 0xff.

			@param number_of_cycles The number of cycles to run the Z80 for.
		*/
		void run_for_cycles(int number_of_cycles) {

#define advance_operation() \
	pc_increment_ = 1;	\
	if(last_request_status_) {	\
		halt_mask_ = 0xff;	\
		if(last_request_status_ & (Interrupt::PowerOn | Interrupt::Reset)) {	\
			request_status_ &= ~Interrupt::PowerOn;	\
			scheduled_program_counter_ = reset_program_.data();	\
		} else if(last_request_status_ & Interrupt::NMI) {	\
			request_status_ &= ~Interrupt::NMI;	\
			scheduled_program_counter_ = nmi_program_.data();	\
		} else if(last_request_status_ & Interrupt::IRQ) {	\
			scheduled_program_counter_ = irq_program_[interrupt_mode_].data();	\
		}	\
	} else {	\
		current_instruction_page_ = &base_page_;	\
		scheduled_program_counter_ = base_page_.fetch_decode_execute_data;	\
	}

			number_of_cycles_ += number_of_cycles;
			if(!scheduled_program_counter_) {
				advance_operation();
			}

			while(1) {

				while(bus_request_line_) {
					static MachineCycle bus_acknowledge_cycle = {MachineCycle::Operation::BusAcknowledge, 1};
					number_of_cycles_ -= static_cast<T *>(this)->perform_machine_cycle(bus_acknowledge_cycle) + 1;
					if(!number_of_cycles_) {
						static_cast<T *>(this)->flush();
						return;
					}
				}

				while(!bus_request_line_) {
					const MicroOp *operation = scheduled_program_counter_;
					scheduled_program_counter_++;

#define set_parity(v)	\
	parity_overflow_result_ = (uint8_t)(v^1);\
	parity_overflow_result_ ^= parity_overflow_result_ >> 4;\
	parity_overflow_result_ ^= parity_overflow_result_ << 2;\
	parity_overflow_result_ ^= parity_overflow_result_ >> 1;

					switch(operation->type) {
						case MicroOp::BusOperation:
							if(number_of_cycles_ < operation->machine_cycle.length) {
								scheduled_program_counter_--;
								static_cast<T *>(this)->flush();
								return;
							}
							if(operation->machine_cycle.was_requested) {
								if(wait_line_) {
									scheduled_program_counter_--;
								} else {
									continue;
								}
							}
							number_of_cycles_ -= operation->machine_cycle.length;
							last_request_status_ = request_status_;
							number_of_cycles_ -= static_cast<T *>(this)->perform_machine_cycle(operation->machine_cycle);
						break;
						case MicroOp::MoveToNextProgram:
							advance_operation();
						break;
						case MicroOp::DecodeOperation:
							ir_.bytes.low = (ir_.bytes.low & 0x80) | ((ir_.bytes.low + current_instruction_page_->r_step) & 0x7f);
							pc_.full += pc_increment_ & (uint16_t)halt_mask_;
						case MicroOp::DecodeOperationNoRChange:
							scheduled_program_counter_ = current_instruction_page_->instructions[operation_ & halt_mask_];
						break;

						case MicroOp::Increment16:			(*(uint16_t *)operation->source)++;											break;
						case MicroOp::IncrementPC:			pc_.full += pc_increment_;													break;
						case MicroOp::Decrement16:			(*(uint16_t *)operation->source)--;											break;
						case MicroOp::Move8:				*(uint8_t *)operation->destination = *(uint8_t *)operation->source;			break;
						case MicroOp::Move16:				*(uint16_t *)operation->destination = *(uint16_t *)operation->source;		break;

						case MicroOp::AssembleAF:
							temp16_.bytes.high = a_;
							temp16_.bytes.low = get_flags();
						break;
						case MicroOp::DisassembleAF:
							a_ = temp16_.bytes.high;
							set_flags(temp16_.bytes.low);
						break;

#pragma mark - Logical

#define set_logical_flags(hf)	\
	sign_result_ = zero_result_ = bit53_result_ = a_;	\
	set_parity(a_);	\
	half_carry_result_ = hf;	\
	subtract_flag_ = 0;	\
	carry_result_ = 0;

						case MicroOp::And:
							a_ &= *(uint8_t *)operation->source;
							set_logical_flags(Flag::HalfCarry);
						break;

						case MicroOp::Or:
							a_ |= *(uint8_t *)operation->source;
							set_logical_flags(0);
						break;

						case MicroOp::Xor:
							a_ ^= *(uint8_t *)operation->source;
							set_logical_flags(0);
						break;

#undef set_logical_flags

						case MicroOp::CPL:
							a_ ^= 0xff;
							subtract_flag_ = Flag::Subtract;
							half_carry_result_ = Flag::HalfCarry;
							bit53_result_ = a_;
						break;

						case MicroOp::CCF:
							half_carry_result_ = (uint8_t)(carry_result_ << 4);
							carry_result_ ^= Flag::Carry;
							subtract_flag_ = 0;
							bit53_result_ = a_;
						break;

						case MicroOp::SCF:
							carry_result_ = Flag::Carry;
							half_carry_result_ = 0;
							subtract_flag_ = 0;
							bit53_result_ = a_;
						break;

#pragma mark - Flow control

						case MicroOp::DJNZ:
							bc_.bytes.high--;
							if(!bc_.bytes.high) {
								advance_operation();
							}
						break;

						case MicroOp::CalculateRSTDestination:
							memptr_.full = operation_ & 0x38;
						break;

#pragma mark - 8-bit arithmetic

#define set_arithmetic_flags(sub, b53)	\
	sign_result_ = zero_result_ = (uint8_t)result;	\
	carry_result_ = (uint8_t)(result >> 8);	\
	half_carry_result_ = (uint8_t)half_result;	\
	parity_overflow_result_ = (uint8_t)(overflow >> 5);	\
	subtract_flag_ = sub;	\
	bit53_result_ = (uint8_t)b53;

						case MicroOp::CP8: {
							uint8_t value = *(uint8_t *)operation->source;
							int result = a_ - value;
							int half_result = (a_&0xf) - (value&0xf);

							// overflow for a subtraction is when the signs were originally
							// different and the result is different again
							int overflow = (value^a_) & (result^a_);

							// the 5 and 3 flags come from the operand, atypically
							set_arithmetic_flags(Flag::Subtract, value);
						} break;

						case MicroOp::SUB8: {
							uint8_t value = *(uint8_t *)operation->source;
							int result = a_ - value;
							int half_result = (a_&0xf) - (value&0xf);

							// overflow for a subtraction is when the signs were originally
							// different and the result is different again
							int overflow = (value^a_) & (result^a_);

							a_ = (uint8_t)result;
							set_arithmetic_flags(Flag::Subtract, result);
						} break;

						case MicroOp::SBC8: {
							uint8_t value = *(uint8_t *)operation->source;
							int result = a_ - value - (carry_result_ & Flag::Carry);
							int half_result = (a_&0xf) - (value&0xf) - (carry_result_ & Flag::Carry);

							// overflow for a subtraction is when the signs were originally
							// different and the result is different again
							int overflow = (value^a_) & (result^a_);

							a_ = (uint8_t)result;
							set_arithmetic_flags(Flag::Subtract, result);
						} break;

						case MicroOp::ADD8: {
							uint8_t value = *(uint8_t *)operation->source;
							int result = a_ + value;
							int half_result = (a_&0xf) + (value&0xf);

							// overflow for addition is when the signs were originally
							// the same and the result is different
							int overflow = ~(value^a_) & (result^a_);

							a_ = (uint8_t)result;
							set_arithmetic_flags(0, result);
						} break;

						case MicroOp::ADC8: {
							uint8_t value = *(uint8_t *)operation->source;
							int result = a_ + value + (carry_result_ & Flag::Carry);
							int half_result = (a_&0xf) + (value&0xf) + (carry_result_ & Flag::Carry);

							// overflow for addition is when the signs were originally
							// the same and the result is different
							int overflow = ~(value^a_) & (result^a_);

							a_ = (uint8_t)result;
							set_arithmetic_flags(0, result);
						} break;

#undef set_arithmetic_flags

						case MicroOp::NEG: {
							int overflow = (a_ == 0x80);
							int result = -a_;
							int halfResult = -(a_&0xf);

							a_ = (uint8_t)result;
							bit53_result_ = sign_result_ = zero_result_ = a_;
							parity_overflow_result_ = overflow ? Flag::Overflow : 0;
							subtract_flag_ = Flag::Subtract;
							carry_result_ = (uint8_t)(result >> 8);
							half_carry_result_ = (uint8_t)halfResult;
						} break;

						case MicroOp::Increment8: {
							uint8_t value = *(uint8_t *)operation->source;
							int result = value + 1;

							// with an increment, overflow occurs if the sign changes from
							// positive to negative
							int overflow = (value ^ result) & ~value;
							int half_result = (value&0xf) + 1;

							*(uint8_t *)operation->source = (uint8_t)result;

							// sign, zero and 5 & 3 are set directly from the result
							bit53_result_ = sign_result_ = zero_result_ = (uint8_t)result;
							half_carry_result_ = (uint8_t)half_result;
							parity_overflow_result_ = (uint8_t)(overflow >> 5);
							subtract_flag_ = 0;
						} break;

						case MicroOp::Decrement8: {
							uint8_t value = *(uint8_t *)operation->source;
							int result = value - 1;

							// with a decrement, overflow occurs if the sign changes from
							// negative to positive
							int overflow = (value ^ result) & value;
							int half_result = (value&0xf) - 1;

							*(uint8_t *)operation->source = (uint8_t)result;

							// sign, zero and 5 & 3 are set directly from the result
							bit53_result_ = sign_result_ = zero_result_ = (uint8_t)result;
							half_carry_result_ = (uint8_t)half_result;
							parity_overflow_result_ = (uint8_t)(overflow >> 5);
							subtract_flag_ = Flag::Subtract;
						} break;

						case MicroOp::DAA: {
							int lowNibble = a_ & 0xf;
							int highNibble = a_ >> 4;
							int amountToAdd = 0;

							if(carry_result_ & Flag::Carry)
							{
								amountToAdd = (lowNibble > 0x9 || (half_carry_result_ & Flag::HalfCarry)) ? 0x66 : 0x60;
							}
							else
							{
								if(half_carry_result_ & Flag::HalfCarry)
								{
									if(lowNibble > 0x9)
										amountToAdd = (highNibble > 0x8) ? 0x66 : 0x06;
									else
										amountToAdd = (highNibble > 0x9) ? 0x66 : 0x06;
								}
								else
								{
									if(lowNibble > 0x9)
										amountToAdd = (highNibble > 0x8) ? 0x66 : 0x06;
									else
										amountToAdd = (highNibble > 0x9) ? 0x60 : 0x00;
								}
							}

							if(!(carry_result_ & Flag::Carry))
							{
								if(lowNibble > 0x9)
								{
									if(highNibble > 0x8) carry_result_ = Flag::Carry;
								}
								else
								{
									if(highNibble > 0x9) carry_result_ = Flag::Carry;
								}
							}

							if(subtract_flag_)
							{
								a_ -= amountToAdd;
								half_carry_result_ = ((half_carry_result_ & Flag::HalfCarry) && lowNibble < 0x6) ? Flag::HalfCarry : 0;
							}
							else
							{
								a_ += amountToAdd;
								half_carry_result_ = (lowNibble > 0x9) ? Flag::HalfCarry : 0;
							}

							sign_result_ = zero_result_ = bit53_result_ = a_;

							set_parity(a_);
						} break;

#pragma mark - 16-bit arithmetic

						case MicroOp::ADD16: {
							memptr_.full = *(uint16_t *)operation->source;
							uint16_t sourceValue = memptr_.full;
							uint16_t destinationValue = *(uint16_t *)operation->destination;
							int result = sourceValue + destinationValue;
							int halfResult = (sourceValue&0xfff) + (destinationValue&0xfff);

							bit53_result_ = (uint8_t)(result >> 8);
							carry_result_ = (uint8_t)(result >> 16);
							half_carry_result_ = (uint8_t)(halfResult >> 8);
							subtract_flag_ = 0;

							*(uint16_t *)operation->destination = (uint16_t)result;
						} break;

						case MicroOp::ADC16: {
							memptr_.full = *(uint16_t *)operation->source;
							uint16_t sourceValue = memptr_.full;
							uint16_t destinationValue = *(uint16_t *)operation->destination;
							int result = sourceValue + destinationValue + (carry_result_ & Flag::Carry);
							int halfResult = (sourceValue&0xfff) + (destinationValue&0xfff) + (carry_result_ & Flag::Carry);

							int overflow = (result ^ destinationValue) & ~(destinationValue ^ sourceValue);

							bit53_result_	=
							sign_result_	= (uint8_t)(result >> 8);
							zero_result_	= (uint8_t)(result | sign_result_);
							subtract_flag_	= 0;
							carry_result_	= (uint8_t)(result >> 16);
							half_carry_result_ = (uint8_t)(halfResult >> 8);
							parity_overflow_result_ = (uint8_t)(overflow >> 13);

							*(uint16_t *)operation->destination = (uint16_t)result;
						} break;

						case MicroOp::SBC16: {
							memptr_.full = *(uint16_t *)operation->source;
							uint16_t sourceValue = memptr_.full;
							uint16_t destinationValue = *(uint16_t *)operation->destination;
							int result = destinationValue - sourceValue - (carry_result_ & Flag::Carry);
							int halfResult = (destinationValue&0xfff) - (sourceValue&0xfff) - (carry_result_ & Flag::Carry);

							// subtraction, so parity rules are:
							// signs of operands were different, 
							// sign of result is different
							int overflow = (result ^ destinationValue) & (sourceValue ^ destinationValue);

							bit53_result_	=
							sign_result_	= (uint8_t)(result >> 8);
							zero_result_	= (uint8_t)(result | sign_result_);
							subtract_flag_	= Flag::Subtract;
							carry_result_	= (uint8_t)(result >> 16);
							half_carry_result_ = (uint8_t)(halfResult >> 8);
							parity_overflow_result_ = (uint8_t)(overflow >> 13);

							*(uint16_t *)operation->destination = (uint16_t)result;
						} break;

#pragma mark - Conditionals

#define decline_conditional()	\
	if(operation->source) {		\
		scheduled_program_counter_ = (MicroOp *)operation->source;	\
	} else {	\
		advance_operation();	\
	}

						case MicroOp::TestNZ:	if(!zero_result_)								{ decline_conditional(); }		break;
						case MicroOp::TestZ:	if(zero_result_)								{ decline_conditional(); }		break;
						case MicroOp::TestNC:	if(carry_result_ & Flag::Carry)					{ decline_conditional(); }		break;
						case MicroOp::TestC:	if(!(carry_result_ & Flag::Carry))				{ decline_conditional(); }		break;
						case MicroOp::TestPO:	if(parity_overflow_result_ & Flag::Parity)		{ decline_conditional(); }		break;
						case MicroOp::TestPE:	if(!(parity_overflow_result_ & Flag::Parity))	{ decline_conditional(); }		break;
						case MicroOp::TestP:	if(sign_result_ & Flag::Sign)					{ decline_conditional(); }		break;
						case MicroOp::TestM:	if(!(sign_result_ & Flag::Sign))				{ decline_conditional(); }		break;

#undef decline_conditional

#pragma mark - Exchange

#define swap(a, b)	temp = a.full; a.full = b.full; b.full = temp;

						case MicroOp::ExDEHL: {
							uint16_t temp;
							swap(de_, hl_);
						} break;

						case MicroOp::ExAFAFDash: {
							uint8_t a = a_;
							uint8_t f = get_flags();
							set_flags(afDash_.bytes.low);
							a_ = afDash_.bytes.high;
							afDash_.bytes.high = a;
							afDash_.bytes.low = f;
						} break;

						case MicroOp::EXX: {
							uint16_t temp;
							swap(de_, deDash_);
							swap(bc_, bcDash_);
							swap(hl_, hlDash_);
						} break;

#undef swap

#pragma mark - Repetition

#define REPEAT(test)	\
	if(test) {	\
		pc_.full -= 2;	\
	} else {	\
		advance_operation();	\
	}

#define LDxR_STEP(dir)	\
	bc_.full--;	\
	de_.full += dir;	\
	hl_.full += dir;	\
	uint8_t sum = a_ + temp8_;	\
	bit53_result_ = (uint8_t)((sum&0x8) | ((sum & 0x02) << 4));	\
	subtract_flag_ = 0;	\
	half_carry_result_ = 0;	\
	parity_overflow_result_ = bc_.full ? Flag::Parity : 0;

						case MicroOp::LDDR: {
							LDxR_STEP(-1);
							REPEAT(bc_.full);
						} break;

						case MicroOp::LDIR: {
							LDxR_STEP(1);
							REPEAT(bc_.full);
						} break;

						case MicroOp::LDD: {
							LDxR_STEP(-1);
						} break;

						case MicroOp::LDI: {
							LDxR_STEP(1);
						} break;

#undef LDxR_STEP

#define CPxR_STEP(dir)	\
	hl_.full += dir;	\
	bc_.full--;	\
	\
	uint8_t result = a_ - temp8_;	\
	uint8_t halfResult = (a_&0xf) - (temp8_&0xf);	\
	\
	parity_overflow_result_ =  bc_.full ? Flag::Parity : 0;	\
	half_carry_result_ = halfResult;	\
	subtract_flag_ = Flag::Subtract;	\
	sign_result_ = zero_result_ = result;	\
	\
	result -= (halfResult >> 4)&1;	\
	bit53_result_ = (uint8_t)((result&0x8) | ((result&0x2) << 4));	\

						case MicroOp::CPDR: {
							CPxR_STEP(-1);
							REPEAT(bc_.full && sign_result_);
						} break;

						case MicroOp::CPIR: {
							CPxR_STEP(1);
							REPEAT(bc_.full && sign_result_);
						} break;

						case MicroOp::CPD: {
							memptr_.full--;
							CPxR_STEP(-1);
						} break;

						case MicroOp::CPI: {
							memptr_.full++;
							CPxR_STEP(1);
						} break;

#undef CPxR_STEP

#define INxR_STEP(dir)	\
	bc_.bytes.high--;	\
	hl_.full += dir;	\
	\
	sign_result_ = zero_result_ = bit53_result_ = bc_.bytes.high;	\
	subtract_flag_ = (temp8_ >> 6) & Flag::Subtract;	\
	\
	int next_bc = bc_.bytes.low + dir;	\
	int summation = temp8_ + (next_bc&0xff);	\
	\
	if(summation > 0xff) {	\
		carry_result_ = Flag::Carry;	\
		half_carry_result_ = Flag::HalfCarry;	\
	} else {	\
		carry_result_ = 0;	\
		half_carry_result_ = 0;	\
	}	\
	\
	summation = (summation&7) ^ bc_.bytes.high;	\
	set_parity(summation);

						case MicroOp::INDR: {
							INxR_STEP(-1);
							REPEAT(bc_.bytes.high);
						} break;

						case MicroOp::INIR: {
							INxR_STEP(1);
							REPEAT(bc_.bytes.high);
						} break;

						case MicroOp::IND: {
							memptr_.full = bc_.full - 1;
							INxR_STEP(-1);
						} break;

						case MicroOp::INI: {
							memptr_.full = bc_.full + 1;
							INxR_STEP(1);
						} break;

#undef INxR_STEP

#define OUTxR_STEP(dir)	\
	bc_.bytes.high--;	\
	hl_.full += dir;	\
	\
	sign_result_ = zero_result_ = bit53_result_ = bc_.bytes.high;	\
	subtract_flag_ = (temp8_ >> 6) & Flag::Subtract;	\
	\
	int summation = temp8_ + hl_.bytes.low;	\
	if(summation > 0xff) {	\
		carry_result_ = Flag::Carry;	\
		half_carry_result_ = Flag::HalfCarry;	\
	} else {	\
		carry_result_ = half_carry_result_ = 0;	\
	}	\
	\
	summation = (summation&7) ^ bc_.bytes.high;	\
	set_parity(summation);

						case MicroOp::OUT_R:
							REPEAT(bc_.bytes.high);
						break;

						case MicroOp::OUTD: {
							OUTxR_STEP(-1);
							memptr_.full = bc_.full - 1;
						} break;

						case MicroOp::OUTI: {
							OUTxR_STEP(1);
							memptr_.full = bc_.full + 1;
						} break;

#undef OUTxR_STEP

#pragma mark - Bit Manipulation

						case MicroOp::BIT: {
							uint8_t result = *(uint8_t *)operation->source & (1 << ((operation_ >> 3)&7));

							if(current_instruction_page_->is_indexed || ((operation_&0x08) == 7)) {
								bit53_result_ = memptr_.bytes.high;
							} else {
								bit53_result_ = *(uint8_t *)operation->source;
							}

							sign_result_ = zero_result_ = result;
							half_carry_result_ = Flag::HalfCarry;
							subtract_flag_ = 0;
							parity_overflow_result_ = result ? 0 : Flag::Parity;
						} break;

						case MicroOp::RES:
							*(uint8_t *)operation->source &= ~(1 << ((operation_ >> 3)&7));
						break;

						case MicroOp::SET:
							*(uint8_t *)operation->source |= (1 << ((operation_ >> 3)&7));
						break;

#pragma mark - Rotation and shifting

#define set_rotate_flags()	\
	bit53_result_ = a_;	\
	carry_result_ = new_carry;	\
	subtract_flag_ = half_carry_result_ = 0;

						case MicroOp::RLA: {
							uint8_t new_carry = a_ >> 7;
							a_ = (uint8_t)((a_ << 1) | (carry_result_ & Flag::Carry));
							set_rotate_flags();
						} break;

						case MicroOp::RRA: {
							uint8_t new_carry = a_ & 1;
							a_ = (uint8_t)((a_ >> 1) | (carry_result_ << 7));
							set_rotate_flags();
						} break;

						case MicroOp::RLCA: {
							uint8_t new_carry = a_ >> 7;
							a_ = (uint8_t)((a_ << 1) | new_carry);
							set_rotate_flags();
						} break;

						case MicroOp::RRCA: {
							uint8_t new_carry = a_ & 1;
							a_ = (uint8_t)((a_ >> 1) | (new_carry << 7));
							set_rotate_flags();
						} break;

#undef set_rotate_flags

#define set_shift_flags()	\
	sign_result_ = zero_result_ = bit53_result_ = *(uint8_t *)operation->source;	\
	set_parity(sign_result_);	\
	half_carry_result_ = 0;	\
	subtract_flag_ = 0;

						case MicroOp::RLC:
							carry_result_ = *(uint8_t *)operation->source >> 7;
							*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source << 1) | carry_result_);
							set_shift_flags();
						break;

						case MicroOp::RRC:
							carry_result_ = *(uint8_t *)operation->source;
							*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1) | (carry_result_ << 7));
							set_shift_flags();
						break;

						case MicroOp::RL: {
							uint8_t next_carry = *(uint8_t *)operation->source >> 7;
							*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source << 1) | (carry_result_ & Flag::Carry));
							carry_result_ = next_carry;
							set_shift_flags();
						} break;

						case MicroOp::RR: {
							uint8_t next_carry = *(uint8_t *)operation->source;
							*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1) | (carry_result_ << 7));
							carry_result_ = next_carry;
							set_shift_flags();
						} break;

						case MicroOp::SLA:
							carry_result_ = *(uint8_t *)operation->source >> 7;
							*(uint8_t *)operation->source = (uint8_t)(*(uint8_t *)operation->source << 1);
							set_shift_flags();
						break;

						case MicroOp::SRA:
							carry_result_ = *(uint8_t *)operation->source;
							*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1) | (*(uint8_t *)operation->source & 0x80));
							set_shift_flags();
						break;

						case MicroOp::SLL:
							carry_result_ = *(uint8_t *)operation->source >> 7;
							*(uint8_t *)operation->source = (uint8_t)(*(uint8_t *)operation->source << 1) | 1;
							set_shift_flags();
						break;

						case MicroOp::SRL:
							carry_result_ = *(uint8_t *)operation->source;
							*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1));
							set_shift_flags();
						break;

#undef set_shift_flags

#define set_decimal_rotate_flags()	\
	subtract_flag_ = 0;	\
	half_carry_result_ = 0;	\
	set_parity(a_);	\
	bit53_result_ = zero_result_ = sign_result_ = a_;

						case MicroOp::RRD: {
							memptr_.full = hl_.full + 1;
							uint8_t low_nibble = a_ & 0xf;
							a_ = (a_ & 0xf0) | (temp8_ & 0xf);
							temp8_ = (uint8_t)((temp8_ >> 4) | (low_nibble << 4));
							set_decimal_rotate_flags();
						} break;

						case MicroOp::RLD: {
							memptr_.full = hl_.full + 1;
							uint8_t low_nibble = a_ & 0xf;
							a_ = (a_ & 0xf0) | (temp8_ >> 4);
							temp8_ = (uint8_t)((temp8_ << 4) | low_nibble);
							set_decimal_rotate_flags();
						} break;

#undef set_decimal_rotate_flags


#pragma mark - Interrupt state

						case MicroOp::EI:
							iff1_ = iff2_ = true;
							if(irq_line_) request_status_ |= Interrupt::IRQ;
						break;

						case MicroOp::DI:
							iff1_ = iff2_ = false;
							request_status_ &= ~Interrupt::IRQ;
						break;

						case MicroOp::IM:
							switch(operation_ & 0x18) {
								case 0x00:	interrupt_mode_ = 0;	break;
								case 0x08:	interrupt_mode_ = 0;	break;	// IM 0/1
								case 0x10:	interrupt_mode_ = 1;	break;
								case 0x18:	interrupt_mode_ = 2;	break;
							}
						break;

#pragma mark - Input

						case MicroOp::SetInFlags:
							subtract_flag_ = half_carry_result_ = 0;
							sign_result_ = zero_result_ = bit53_result_ = *(uint8_t *)operation->source;
							set_parity(sign_result_);
						break;

						case MicroOp::SetAFlags:
							subtract_flag_ = half_carry_result_ = 0;
							parity_overflow_result_ = iff2_ ? Flag::Parity : 0;
							sign_result_ = zero_result_ = bit53_result_ = a_;
						break;

						case MicroOp::SetZero:
							temp8_ = 0;
						break;

#pragma mark - Special-case Flow

						case MicroOp::BeginIRQMode0:
							pc_increment_ = 0;			// deliberate fallthrough
						case MicroOp::BeginIRQ:
							iff2_ = iff1_ = false;
							request_status_ &= ~Interrupt::IRQ;
							temp16_.full = 0x38;
						break;

						case MicroOp::BeginNMI:
							iff2_ = iff1_;
							iff1_ = false;
							request_status_ &= ~Interrupt::IRQ;
						break;

						case MicroOp::JumpTo66:
							pc_.full = 0x66;
						break;

						case MicroOp::RETN:
							iff1_ = iff2_;
							if(irq_line_ && iff1_) request_status_ |= Interrupt::IRQ;
						break;

						case MicroOp::HALT:
							halt_mask_ = 0x00;
						break;

#pragma mark - Interrupt handling

						case MicroOp::Reset:
							iff1_ = iff2_ = false;
							interrupt_mode_ = 0;
							pc_.full = 0;
							sp_.full = 0xffff;
							a_ = 0xff;
							set_flags(0xff);
							ir_.full = 0;
						break;

#pragma mark - Internal bookkeeping

						case MicroOp::SetInstructionPage:
							current_instruction_page_ = (InstructionPage *)operation->source;
							scheduled_program_counter_ = current_instruction_page_->fetch_decode_execute_data;
						break;

						case MicroOp::CalculateIndexAddress:
							memptr_.full = (uint16_t)(*(uint16_t *)operation->source + (int8_t)temp8_);
						break;

						case MicroOp::IndexedPlaceHolder:
							printf("Hit placeholder!!!\n");
						return;
					}
#undef set_parity
				}

			}
		}

		/*!
			Called to announce the end of a run_for_cycles period, allowing deferred work to take place.

			Users of the Z80 template may override this.
		*/
		void flush() {}

		int perform_machine_cycle(const MachineCycle &cycle) {
			return 0;
		}

		/*!
			Gets the flags register.

			@see set_flags

			@returns The current value of the flags register.
		*/
		uint8_t get_flags() {
			uint8_t result =
				(sign_result_ & Flag::Sign) |
				(zero_result_ ? 0 : Flag::Zero) |
				(bit53_result_ & (Flag::Bit5 | Flag::Bit3)) |
				(half_carry_result_ & Flag::HalfCarry) |
				(parity_overflow_result_ & Flag::Parity) |
				subtract_flag_ |
				(carry_result_ & Flag::Carry);
			return result;
		}

		/*!
			Sets the flags register.

			@see set_flags

			@param flags The new value of the flags register.
		*/
		void set_flags(uint8_t flags) {
			sign_result_			= flags;
			zero_result_			= (flags & Flag::Zero) ^ Flag::Zero;
			bit53_result_			= flags;
			half_carry_result_		= flags;
			parity_overflow_result_	= flags;
			subtract_flag_			= flags & Flag::Subtract;
			carry_result_			= flags;
		}

		/*!
			Gets the value of a register.

			@see set_value_of_register

			@param r The register to set.
			@returns The value of the register. 8-bit registers will be returned as unsigned.
		*/
		uint16_t get_value_of_register(Register r) {
			switch (r) {
				case Register::ProgramCounter:			return pc_.full;
				case Register::StackPointer:			return sp_.full;

				case Register::A:						return a_;
				case Register::Flags:					return get_flags();
				case Register::AF:						return (uint16_t)((a_ << 8) | get_flags());
				case Register::B:						return bc_.bytes.high;
				case Register::C:						return bc_.bytes.low;
				case Register::BC:						return bc_.full;
				case Register::D:						return de_.bytes.high;
				case Register::E:						return de_.bytes.low;
				case Register::DE:						return de_.full;
				case Register::H:						return hl_.bytes.high;
				case Register::L:						return hl_.bytes.low;
				case Register::HL:						return hl_.full;

				case Register::ADash:					return afDash_.bytes.high;
				case Register::FlagsDash:				return afDash_.bytes.low;
				case Register::AFDash:					return afDash_.full;
				case Register::BDash:					return bcDash_.bytes.high;
				case Register::CDash:					return bcDash_.bytes.low;
				case Register::BCDash:					return bcDash_.full;
				case Register::DDash:					return deDash_.bytes.high;
				case Register::EDash:					return deDash_.bytes.low;
				case Register::DEDash:					return deDash_.full;
				case Register::HDash:					return hlDash_.bytes.high;
				case Register::LDash:					return hlDash_.bytes.low;
				case Register::HLDash:					return hlDash_.full;

				case Register::IXh:						return ix_.bytes.high;
				case Register::IXl:						return ix_.bytes.low;
				case Register::IX:						return ix_.full;
				case Register::IYh:						return iy_.bytes.high;
				case Register::IYl:						return iy_.bytes.low;
				case Register::IY:						return iy_.full;

				case Register::R:						return ir_.bytes.low;
				case Register::I:						return ir_.bytes.high;
				case Register::Refresh:					return ir_.full;

				case Register::IFF1:					return iff1_ ? 1 : 0;
				case Register::IFF2:					return iff2_ ? 1 : 0;
				case Register::IM:						return (uint16_t)interrupt_mode_;

				default: return 0;
			}
		}

		/*!
			Sets the value of a register.

			@see get_value_of_register

			@param r The register to set.
			@param value The value to set. If the register is only 8 bit, the value will be truncated.
		*/
		void set_value_of_register(Register r, uint16_t value) {
			switch (r) {
				case Register::ProgramCounter:	pc_.full = value;				break;
				case Register::StackPointer:	sp_.full = value;				break;

				case Register::A:				a_ = (uint8_t)value;			break;
				case Register::AF:				a_ = (uint8_t)(value >> 8);		// deliberate fallthrough...
				case Register::Flags:			set_flags((uint8_t)value);		break;

				case Register::B:				bc_.bytes.high = (uint8_t)value;		break;
				case Register::C:				bc_.bytes.low = (uint8_t)value;			break;
				case Register::BC:				bc_.full = value;						break;
				case Register::D:				de_.bytes.high = (uint8_t)value;		break;
				case Register::E:				de_.bytes.low = (uint8_t)value;			break;
				case Register::DE:				de_.full = value;						break;
				case Register::H:				hl_.bytes.high = (uint8_t)value;		break;
				case Register::L:				hl_.bytes.low = (uint8_t)value;			break;
				case Register::HL:				hl_.full = value;						break;

				case Register::ADash:			afDash_.bytes.high = (uint8_t)value;	break;
				case Register::FlagsDash:		afDash_.bytes.low = (uint8_t)value;		break;
				case Register::AFDash:			afDash_.full = value;					break;
				case Register::BDash:			bcDash_.bytes.high = (uint8_t)value;	break;
				case Register::CDash:			bcDash_.bytes.low = (uint8_t)value;		break;
				case Register::BCDash:			bcDash_.full = value;					break;
				case Register::DDash:			deDash_.bytes.high = (uint8_t)value;	break;
				case Register::EDash:			deDash_.bytes.low = (uint8_t)value;		break;
				case Register::DEDash:			deDash_.full = value;					break;
				case Register::HDash:			hlDash_.bytes.high = (uint8_t)value;	break;
				case Register::LDash:			hlDash_.bytes.low = (uint8_t)value;		break;
				case Register::HLDash:			hlDash_.full = value;					break;

				case Register::IXh:				ix_.bytes.high = (uint8_t)value;		break;
				case Register::IXl:				ix_.bytes.low = (uint8_t)value;			break;
				case Register::IX:				ix_.full = value;						break;
				case Register::IYh:				iy_.bytes.high = (uint8_t)value;		break;
				case Register::IYl:				iy_.bytes.low = (uint8_t)value;			break;
				case Register::IY:				iy_.full = value;						break;

				case Register::R:				ir_.bytes.low = (uint8_t)value;			break;
				case Register::I:				ir_.bytes.high = (uint8_t)value;		break;
				case Register::Refresh:			ir_.full = (uint16_t)value;				break;

				case Register::IFF1:			iff1_ = !!value;						break;
				case Register::IFF2:			iff2_ = !!value;						break;
				case Register::IM:				interrupt_mode_ = value % 3;			break;

				default: break;
			}
		}

		/*!
			Gets the value of the HALT output line.
		*/
		bool get_halt_line() {
			return halt_mask_ == 0x00;
		}

		/*!
			Sets the logical value of the interrupt line.

			@param offset If called while within perform_machine_cycle this may be a value indicating
			how many cycles before now the line changed state. The value may not be longer than the
			current machine cycle. If called at any other time, this must be zero.
		*/
		void set_interrupt_line(bool value, int offset = 0) {
			if(irq_line_ == value) return;

			// IRQ requests are level triggered and masked.
			irq_line_ = value;
			if(irq_line_ && iff1_) {
				request_status_ |= Interrupt::IRQ;
			} else {
				request_status_ &= ~Interrupt::IRQ;
			}

			// If this change happened at least one cycle ago then: (i) we're promised that this is a machine
			// cycle per the contract on supplying an offset; and (ii) that means it happened before the lines
			// were sampled. So adjust the most recent sample.
			if(offset < 0) {
				last_request_status_ = (last_request_status_ & ~Interrupt::IRQ) | (request_status_ & Interrupt::IRQ);
			}
		}

		/*!
			Sets the logical value of the non-maskable interrupt line.
			
			@param offset See discussion in set_interrupt_line.
		*/
		void set_non_maskable_interrupt_line(bool value, int offset = 0) {
			// NMIs are edge triggered and cannot be masked.
			if(value) {
				request_status_ |= Interrupt::NMI;
				if(offset < 0) {
					last_request_status_ |= Interrupt::NMI;
				}
			}
		}

		/*!
			Sets the logical value of the bus request line.
		*/
		void set_bus_request_line(bool value) {
			bus_request_line_ = value;
		}

		/*!
			Sets the logical value of the reset line.
		*/
		void set_reset_line(bool value) {
			// Reset requests are level triggered and cannot be masked.
			if(value) request_status_ |= Interrupt::Reset;
			else request_status_ &= ~Interrupt::Reset;
		}

		/*!
			This emulation automatically sets itself up in power-on state at creation, which has the effect of triggering a
			reset at the first opportunity. Use @c reset_power_on to disable that behaviour.
		*/
		inline void reset_power_on() {
			request_status_ &= ~Interrupt::PowerOn;
			last_request_status_ &= ~Interrupt::PowerOn;
		}

		/*!
			Sets the logical value of the wait line.
		*/
		inline void set_wait_line(bool value) {
			wait_line_ = value;
		}

		/*!
			For receivers of perform_machine_cycle only. Temporarily rejects the current machine
			cycle, causing time to be rewinded to its beginning.

			Behaviour will be to cause the Z80 to repeat this machine cycle, having adjusted total
			running time appropriately. This method is intended for use if a subclass receives
			perform_machine_cycle, indicating that the cycle should be completed, and discovers
			that it should have signalled IRQ, NMI or BUSREQ during the cycle. In that case it
			can rewind time and post the signal with a proper time offset.
		*/
//		void reject_machine_cycle() {
//		}

		/*!
			Returns the bus cycle that the Z80 is currently in the process of performing.
		*/
//		const MachineCycle &get_current_bus_cycle(int &cycles_since_start) {
//		}
};

}
}

#endif /* Z80_hpp */
