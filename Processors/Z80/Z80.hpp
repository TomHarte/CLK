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

#include "../MicroOpScheduler.hpp"
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
	R,		I,

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

	@c None is reserved for internal use. It will never be requested from a subclass.
*/
enum BusOperation {
	ReadOpcode = 0,
	Read, Write,
	Input, Output,
	Interrupt,
//	BusRequest, BusAcknowledge,
	Internal
};

struct MachineCycle {
	BusOperation operation;
	int length;
	uint16_t *address;
	uint8_t *value;
};

struct MicroOp {
	enum Type {
		BusOperation,
		DecodeOperation,
		MoveToNextProgram,

		Increment8,
		Increment16,
		Decrement8,
		Decrement16,
		Move8,
		Move16,

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

		ExDEHL, ExAFAFDash,

		EI,		DI,

		LDIR,

		RLA,	RLCA,	RRA,	RRCA,
		RLC,	RRC,	RL,		RR,
		SLA,	SRA,	SLL,	SRL,

		SetInstructionPage,
		CalculateIndexAddress,

		DJNZ,
		DAA,
		CPL,
		SCF,
		CCF,

		RES,
		BIT,
		SET,

		CalculateRSTDestination,

		IndexedPlaceHolder,

		None
	};
	Type type;
	void *source;
	void *destination;
	MachineCycle machine_cycle;
};

/*!
	@abstact An abstract base class for emulation of a 6502 processor via the curiously recurring template pattern/f-bounded polymorphism.

	@discussion Subclasses should implement @c perform_bus_operation(BusOperation operation, uint16_t address, uint8_t *value) in
	order to provide the bus on which the 6502 operates and @c flush(), which is called upon completion of a continuous run
	of cycles to allow a subclass to bring any on-demand activities up to date.

	Additional functionality can be provided by the host machine by providing a jam handler and inserting jam opcodes where appropriate;
	that will cause call outs when the program counter reaches those addresses. @c return_from_subroutine can be used to exit from a
	jammed state.
*/
template <class T> class Processor: public MicroOpScheduler<MicroOp> {
	private:
		uint8_t a_, i_, r_;
		RegisterPair bc_, de_, hl_;
		RegisterPair afDash_, bcDash_, deDash_, hlDash_;
		RegisterPair ix_, iy_, pc_, sp_;
		bool iff1_, iff2_;
		int interrupt_mode_;
		uint8_t sign_result_, zero_result_, bit5_result_, half_carry_flag_, bit3_result_, parity_overflow_flag_, subtract_flag_, carry_flag_;

		int number_of_cycles_;

		uint8_t operation_;
		RegisterPair temp16_;
		uint8_t temp8_;

		MicroOp *fetch_decode_execute_;
		struct InstructionPage {
			MicroOp *instructions[256];
			MicroOp *all_operations;
			bool increments_r;

			InstructionPage() : all_operations(nullptr), increments_r(true) {
				for(int c = 0; c < 256; c++) {
					instructions[c] = nullptr;
				}
			}

			~InstructionPage() {
				delete[] all_operations;
			}
		};
		InstructionPage *current_instruction_page_;

		InstructionPage base_page_;
		InstructionPage ed_page_;
		InstructionPage fd_page_;
		InstructionPage dd_page_;

		InstructionPage cb_page_;
		InstructionPage fdcb_page_;
		InstructionPage ddcb_page_;

#define XX				{MicroOp::None, 0}

#define INDEX()			{MicroOp::IndexedPlaceHolder}, FETCH(temp8_, pc_), WAIT(5), {MicroOp::CalculateIndexAddress, &index}
#define FINDEX()		{MicroOp::IndexedPlaceHolder}, FETCH(temp8_, pc_), {MicroOp::CalculateIndexAddress, &index}
#define INDEX_ADDR()	(add_offsets ? temp16_ : index)

/// Fetches into x from address y, and then increments y.
#define FETCH(x, y)		{MicroOp::BusOperation, nullptr, nullptr, {Read, 3, &y.full, &x}}, {MicroOp::Increment16, &y.full}
/// Fetches into x from address y.
#define FETCHL(x, y)	{MicroOp::BusOperation, nullptr, nullptr, {Read, 3, &y.full, &x}}

/// Stores x to address y, and then increments y.
#define STORE(x, y)		{MicroOp::BusOperation, nullptr, nullptr, {Write, 3, &y.full, &x}}, {MicroOp::Increment16, &y.full}
/// Stores x to address y.
#define STOREL(x, y)	{MicroOp::BusOperation, nullptr, nullptr, {Write, 3, &y.full, &x}}

/// Fetches the 16-bit quantity x from address y, incrementing y twice.
#define FETCH16(x, y)	FETCH(x.bytes.low, y), FETCH(x.bytes.high, y)
/// Fetches the 16-bit quantity x from address y, incrementing y once.
#define FETCH16L(x, y)	FETCH(x.bytes.low, y), FETCHL(x.bytes.high, y)

/// Stores the 16-bit quantity x to address y, incrementing y once.
#define STORE16L(x, y)	STORE(x.bytes.low, y), STOREL(x.bytes.high, y)

#define PUSH(x)			{MicroOp::Decrement16, &sp_.full}, STOREL(x.bytes.high, sp_), {MicroOp::Decrement16, &sp_.full}, STOREL(x.bytes.low, sp_)
#define POP(x)			FETCHL(x.bytes.low, sp_), {MicroOp::Increment16, &sp_.full}, FETCHL(x.bytes.high, sp_), {MicroOp::Increment16, &sp_.full}

#define JP(cc)			Program(FETCH16(temp16_, pc_), {MicroOp::cc}, {MicroOp::Move16, &temp16_.full, &pc_.full})
#define CALL(cc)		Program(FETCH16(temp16_, pc_), {MicroOp::cc}, WAIT(1), PUSH(pc_), {MicroOp::Move16, &temp16_.full, &pc_.full})
#define RET(cc)			Program(WAIT(1), {MicroOp::cc}, POP(pc_))
#define JR(cc)			Program(FETCH(temp8_, pc_), {MicroOp::cc}, WAIT(5), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &temp16_.full, &pc_.full})
#define RST()			Program(WAIT(1), {MicroOp::CalculateRSTDestination}, PUSH(pc_), {MicroOp::Move16, &temp16_.full, &pc_.full})
#define LD(a, b)		Program({MicroOp::Move8, &b, &a})

#define LD_GROUP(r, ri)	\
				LD(r, bc_.bytes.high),		LD(r, bc_.bytes.low),	LD(r, de_.bytes.high),						LD(r, de_.bytes.low),	\
				LD(r, index.bytes.high),	LD(r, index.bytes.low),	Program(INDEX(), FETCHL(ri, INDEX_ADDR())),	LD(r, a_)

#define READ_OP_GROUP(op)	\
				Program({MicroOp::op, &bc_.bytes.high}),	Program({MicroOp::op, &bc_.bytes.low}),	\
				Program({MicroOp::op, &de_.bytes.high}),	Program({MicroOp::op, &de_.bytes.low}),	\
				Program({MicroOp::op, &index.bytes.high}),	Program({MicroOp::op, &index.bytes.low}),	\
				Program(INDEX(), FETCHL(temp8_, INDEX_ADDR()), {MicroOp::op, &temp8_}),	\
				Program({MicroOp::op, &a_})

#define RMW(x, op, ...) Program(INDEX(), FETCHL(x, INDEX_ADDR()), {MicroOp::op, &x}, WAIT(1), STOREL(x, INDEX_ADDR()))
#define RMWI(x, op, ...) Program(WAIT(1), FETCHL(x, INDEX_ADDR()), {MicroOp::op, &x}, WAIT(1), STOREL(x, INDEX_ADDR()))

#define MODIFY_OP_GROUP(op)	\
				Program({MicroOp::op, &bc_.bytes.high}),	Program({MicroOp::op, &bc_.bytes.low}),	\
				Program({MicroOp::op, &de_.bytes.high}),	Program({MicroOp::op, &de_.bytes.low}),	\
				Program({MicroOp::op, &index.bytes.high}),	Program({MicroOp::op, &index.bytes.low}),	\
				RMW(temp8_, op),	\
				Program({MicroOp::op, &a_})

#define MUTATE_OP_GROUP(op)	\
				RMWI(bc_.bytes.high, op),	\
				RMWI(bc_.bytes.low, op),	\
				RMWI(de_.bytes.high, op),	\
				RMWI(de_.bytes.low, op),	\
				RMWI(hl_.bytes.high, op),	\
				RMWI(hl_.bytes.low, op),	\
				RMWI(temp8_, op),	\
				RMWI(a_, op)


#define ADD16(d, s) Program(WAIT(4), WAIT(3), {MicroOp::ADD16, &s.full, &d.full})
#define ADC16(d, s) Program(WAIT(4), WAIT(3), {MicroOp::ADC16, &s.full, &d.full})
#define SBC16(d, s) Program(WAIT(4), WAIT(3), {MicroOp::SBC16, &s.full, &d.full})

#define WAIT(n)			{MicroOp::BusOperation, nullptr, nullptr, {Internal, n} }
#define Program(...)	{ __VA_ARGS__, {MicroOp::MoveToNextProgram} }

		typedef MicroOp InstructionTable[256][20];

		void assemble_page(InstructionPage &target, InstructionTable &table, bool add_offsets) {
			size_t number_of_micro_ops = 0;
			size_t lengths[256];

			// Count number of micro-ops required.
			for(int c = 0; c < 256; c++) {
				size_t length = 0;
				while(table[c][length].type != MicroOp::MoveToNextProgram && table[c][length].type != MicroOp::None) length++;
				length++;
				lengths[c] = length;
				number_of_micro_ops += length;
			}

			// Allocate a landing area.
			target.all_operations = new MicroOp[number_of_micro_ops];

			// Copy in all programs and set pointers.
			size_t destination = 0;
			for(int c = 0; c < 256; c++) {
				target.instructions[c] = &target.all_operations[destination];
				for(int t = 0; t < lengths[c];) {
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
#define NOP_ROW()	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX,	XX
			InstructionTable ed_program_table = {
				NOP_ROW(),	/* 0x00 */
				NOP_ROW(),	/* 0x10 */
				NOP_ROW(),	/* 0x20 */
				NOP_ROW(),	/* 0x30 */
				/* 0x40 IN B, (C) */	XX,								/* 0x41 OUT (C), B */	XX,
				/* 0x42 SBC HL, BC */	SBC16(hl_, bc_),				/* 0x43 LD (nn), BC */	XX,
				/* 0x44 NEG */			XX,								/* 0x45 RETN */			XX,
				/* 0x46 IM 0 */			XX,								/* 0x47 LD I, A */		XX,
				/* 0x48 IN C, (C) */	XX,								/* 0x49 OUT (C), C */	XX,
				/* 0x4a ADC HL, BC */	ADC16(hl_, bc_),				/* 0x4b LD BC, (nn) */	XX,
				/* 0x4c NEG */			XX,								/* 0x4d RETI */			XX,
				/* 0x4e IM 0/1 */		XX,								/* 0x4f LD R, A */		XX,
				/* 0x50 IN D, (C) */	XX,								/* 0x51 OUT (C), D */	XX,
				/* 0x52 SBC HL, DE */	SBC16(hl_, de_),				/* 0x53 LD (nn), DE */	XX,
				/* 0x54 NEG */			XX,								/* 0x55 RETN */			XX,
				/* 0x56 IM 1 */			XX,								/* 0x57 LD A, I */		XX,
				/* 0x58 IN E, (C) */	XX,								/* 0x59 OUT (C), E */	XX,
				/* 0x5a ADC HL, DE */	ADC16(hl_, de_),				/* 0x5b LD DE, (nn) */	XX,
				/* 0x5c NEG */			XX,								/* 0x5d RETN */			XX,
				/* 0x5e IM 2 */			XX,								/* 0x5f LD A, R */		XX,
				/* 0x60 IN H, (C) */	XX,								/* 0x61 OUT (C), H */	XX,
				/* 0x62 SBC HL, HL */	SBC16(hl_, hl_),				/* 0x63 LD (nn), HL */	XX,
				/* 0x64 NEG */			XX,								/* 0x65 RETN */			XX,
				/* 0x66 IM 0 */			XX,								/* 0x67 RRD */			XX,
				/* 0x68 IN L, (C) */	XX,								/* 0x69 OUT (C), L */	XX,
				/* 0x6a ADC HL, HL */	ADC16(hl_, hl_),				/* 0x6b LD HL, (nn) */	XX,
				/* 0x6c NEG */			XX,								/* 0x6d RETN */			XX,
				/* 0x6e IM 0/1 */		XX,								/* 0x6f RLD */			XX,
				/* 0x70 IN (C) */		XX,								/* 0x71 OUT (C), 0 */	XX,
				/* 0x72 SBC HL, SP */	SBC16(hl_, sp_),				/* 0x73 LD (nn), SP */	Program(FETCH16(temp16_, pc_), STORE16L(sp_, temp16_)),
				/* 0x74 NEG */			XX,								/* 0x75 RETN */			XX,
				/* 0x76 IM 1 */			XX,								/* 0x77 XX */			XX,
				/* 0x78 IN A, (C) */	XX,								/* 0x79 OUT (C), A */	XX,
				/* 0x7a ADC HL, SP */	ADC16(hl_, sp_),				/* 0x7b LD SP, (nn) */	Program(FETCH16(temp16_, pc_), FETCH16L(sp_, temp16_)),
				/* 0x7c NEG */			XX,								/* 0x7d RETN */			XX,
				/* 0x7e IM 2 */			XX,								/* 0x7f XX */			XX,
				NOP_ROW(),	/* 0x80 */
				NOP_ROW(),	/* 0x90 */
				/* 0xa0 LDI */		XX,
				/* 0xa1 CPI */		XX,
				/* 0xa2 INI */		XX,								/* 0xa3 OTI */		XX,
				XX, XX, XX, XX,
				/* 0xa8 LDD */		XX,								/* 0xa9 CPD */		XX,
				/* 0xaa IND */		XX,								/* 0xab OTD */		XX,
				XX, XX, XX, XX,
				/* 0xb0 LDIR */		Program(FETCHL(temp8_, hl_), STOREL(temp8_, de_), WAIT(2), {MicroOp::LDIR}, WAIT(5)),
				/* 0xb1 CPIR */		XX,
				/* 0xb2 INIR */		XX,								/* 0xb3 OTIR */		XX,
				XX, XX, XX, XX,
				/* 0xb8 LDDR */		XX,								/* 0xb9 CPDR */		XX,
				/* 0xba INDR */		XX,								/* 0xbb OTDR */		XX,
				XX, XX, XX, XX,
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
#define CB_PAGE(m)	m(RLC), m(RRC),	m(RL),	m(RR),	m(SLA),	m(SRA),	m(SLL),	m(SRL),	OCTO_OP_GROUP(m, BIT),	OCTO_OP_GROUP(m, RES),	OCTO_OP_GROUP(m, SET)

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
				CB_PAGE(MODIFY_OP_GROUP)
			};
			InstructionTable offsets_cb_program_table = {
				CB_PAGE(MUTATE_OP_GROUP)
			};
			assemble_page(target, add_offsets ? offsets_cb_program_table : cb_program_table, add_offsets);

#undef OCTO_OP_GROUP
#undef CB_PAGE
		}

		void assemble_base_page(InstructionPage &target, RegisterPair &index, bool add_offsets, InstructionPage &cb_page) {
#define INC_DEC_LD(r)	\
				Program({MicroOp::Increment8, &r}),	\
				Program({MicroOp::Decrement8, &r}),	\
				Program(FETCH(r, pc_))

#define INC_INC_DEC_LD(rf, r)	\
				Program(WAIT(2), {MicroOp::Increment16, &rf.full}), INC_DEC_LD(r)

#define DEC_INC_DEC_LD(rf, r)	\
				Program(WAIT(2), {MicroOp::Decrement16, &rf.full}), INC_DEC_LD(r)

			InstructionTable base_program_table = {
				/* 0x00 NOP */			{ {MicroOp::MoveToNextProgram} },	/* 0x01 LD BC, nn */	Program(FETCH16(bc_, pc_)),
				/* 0x02 LD (BC), A */	Program(STOREL(a_, bc_)),

				/* 0x03 INC BC;	0x04 INC B;	0x05 DEC B;	0x06 LD B, n */
				INC_INC_DEC_LD(bc_, bc_.bytes.high),

				/* 0x07 RLCA */			Program({MicroOp::RLCA}),
				/* 0x08 EX AF, AF' */	Program({MicroOp::ExAFAFDash}),		/* 0x09 ADD HL, BC */	ADD16(index, bc_),
				/* 0x0a LD A, (BC) */	Program(FETCHL(a_, bc_)),

				/* 0x0b DEC BC;	0x0c INC C; 0x0d DEC C; 0x0e LD C, n */
				DEC_INC_DEC_LD(bc_, bc_.bytes.low),

				/* 0x0f RRCA */			Program({MicroOp::RRCA}),
				/* 0x10 DJNZ */			Program(WAIT(1), FETCH(temp8_, pc_), {MicroOp::DJNZ}, WAIT(5), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &temp16_.full, &pc_.full}),
				/* 0x11 LD DE, nn */	Program(FETCH16(de_, pc_)),
				/* 0x12 LD (DE), A */	Program(STOREL(a_, de_)),

				/* 0x13 INC DE;	0x14 INC D;	0x15 DEC D;	0x16 LD D, n */
				INC_INC_DEC_LD(de_, de_.bytes.high),

				/* 0x17 RLA */			Program({MicroOp::RLA}),
				/* 0x18 JR */			Program(FETCH(temp8_, pc_), WAIT(5), {MicroOp::CalculateIndexAddress, &pc_.full}, {MicroOp::Move16, &temp16_.full, &pc_.full}),
				/* 0x19 ADD HL, DE */	ADD16(index, de_),
				/* 0x1a LD A, (DE) */	Program(FETCHL(a_, de_)),

				/* 0x1b DEC DE;	0x1c INC E; 0x1d DEC E; 0x1e LD E, n */
				DEC_INC_DEC_LD(de_, de_.bytes.low),

				/* 0x1f RRA */			Program({MicroOp::RRA}),
				/* 0x20 JR NZ */		JR(TestNZ),							 /* 0x21 LD HL, nn */	Program(FETCH16(index, pc_)),
				/* 0x22 LD (nn), HL */	Program(FETCH16(temp16_, pc_), STORE16L(index, temp16_)),

				/* 0x23 INC HL;	0x24 INC H;	0x25 DEC H;	0x26 LD H, n */
				INC_INC_DEC_LD(index, index.bytes.high),

				/* 0x27 DAA */			Program({MicroOp::DAA}),
				/* 0x28 JR Z */			JR(TestZ),							/* 0x29 ADD HL, HL */	ADD16(index, index),
				/* 0x2a LD HL, (nn) */	Program(FETCH16(temp16_, pc_), FETCH16L(index, temp16_)),

				/* 0x2b DEC HL;	0x2c INC L; 0x2d DEC L; 0x2e LD L, n */
				DEC_INC_DEC_LD(index, index.bytes.low),

				/* 0x2f CPL */			Program({MicroOp::CPL}),
				/* 0x30 JR NC */		JR(TestNC),							/* 0x31 LD SP, nn */	Program(FETCH16(sp_, pc_)),
				/* 0x32 LD (nn), A */	Program(FETCH16(temp16_, pc_), STOREL(a_, temp16_)),
				/* 0x33 INC SP */		Program(WAIT(2), {MicroOp::Increment16, &sp_.full}),
				/* 0x34 INC (HL) */		Program(INDEX(), FETCHL(temp8_, INDEX_ADDR()), WAIT(1), {MicroOp::Increment8, &temp8_}, STOREL(temp8_, INDEX_ADDR())),
				/* 0x35 DEC (HL) */		Program(INDEX(), FETCHL(temp8_, INDEX_ADDR()), WAIT(1), {MicroOp::Decrement8, &temp8_}, STOREL(temp8_, INDEX_ADDR())),
				/* 0x36 LD (HL), n */	Program(INDEX(), FETCH(temp8_, pc_), STOREL(temp8_, INDEX_ADDR())),
				/* 0x37 SCF */			Program({MicroOp::SCF}),
				/* 0x38 JR C */			JR(TestC),
				/* 0x39 ADD HL, SP */	ADD16(index, sp_),
				/* 0x3a LD A, (nn) */	Program(FETCH16(temp16_, pc_), FETCHL(a_, temp16_)),
				/* 0x3b DEC SP */		Program(WAIT(2), {MicroOp::Decrement16, &sp_.full}),

				/* 0x3c INC A;	0x3d DEC A;	0x3e LD A, n */
				INC_DEC_LD(a_),

				/* 0x3f CCF */			Program({MicroOp::CCF}),

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

				/* 0x70 LD (HL),B */	Program(INDEX(), STOREL(bc_.bytes.high, INDEX_ADDR())),
				/* 0x71 LD (HL), C */	Program(INDEX(), STOREL(bc_.bytes.low, INDEX_ADDR())),
				/* 0x72 LD (HL),D */	Program(INDEX(), STOREL(de_.bytes.high, INDEX_ADDR())),
				/* 0x73 LD (HL), E */	Program(INDEX(), STOREL(de_.bytes.low, INDEX_ADDR())),
				/* 0x74 LD (HL),H */	Program(INDEX(), STOREL(index.bytes.high, INDEX_ADDR())),
				/* 0x75 LD (HL), L */	Program(INDEX(), STOREL(index.bytes.low, INDEX_ADDR())),
				/* 0x76 HALT */			XX,
				/* 0x77 LD (HL), A */	Program(INDEX(), STOREL(a_, INDEX_ADDR())),

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

				/* 0xc0 RET NZ */	RET(TestNZ),							/* 0xc1 POP BC */	Program(POP(bc_)),
				/* 0xc2 JP NZ */	JP(TestNZ),								/* 0xc3 JP nn */	Program(FETCH16L(temp16_, pc_), {MicroOp::Move16, &temp16_.full, &pc_.full}),
				/* 0xc4 CALL NZ */	CALL(TestNZ),							/* 0xc5 PUSH BC */	Program(WAIT(1), PUSH(bc_)),
				/* 0xc6 ADD A, n */	Program(FETCH(temp8_, pc_), {MicroOp::ADD8, &temp8_}),
				/* 0xc7 RST 00h */	RST(),
				/* 0xc8 RET Z */	RET(TestZ),								/* 0xc9 RET */		Program(POP(pc_)),
				/* 0xca JP Z */		JP(TestZ),								/* 0xcb [CB page] */Program({MicroOp::SetInstructionPage, &cb_page}, FINDEX()),
				/* 0xcc CALL Z */	CALL(TestZ),							/* 0xcd CALL */		Program(FETCH16(temp16_, pc_), WAIT(1), PUSH(pc_), {MicroOp::Move16, &temp16_.full, &pc_.full}),
				/* 0xce ADC A, n */	Program(FETCH(temp8_, pc_), {MicroOp::ADC8, &temp8_}),
				/* 0xcf RST 08h */	RST(),
				/* 0xd0 RET NC */	RET(TestNC),							/* 0xd1 POP DE */	Program(POP(de_)),
				/* 0xd2 JP NC */	JP(TestNC),								/* 0xd3 OUT (n), A */XX,
				/* 0xd4 CALL NC */	CALL(TestNC),							/* 0xd5 PUSH DE */	Program(WAIT(1), PUSH(de_)),
				/* 0xd6 SUB n */	Program(FETCH(temp8_, pc_), {MicroOp::SUB8, &temp8_}),
				/* 0xd7 RST 10h */	RST(),
				/* 0xd8 RET C */	RET(TestC),								/* 0xd9 EXX */		XX,
				/* 0xda JP C */		JP(TestC),								/* 0xdb IN A, (n) */XX,
				/* 0xdc CALL C */	CALL(TestC),							/* 0xdd [DD page] */Program({MicroOp::SetInstructionPage, &dd_page_}),
				/* 0xde SBC A, n */	Program(FETCH(temp8_, pc_), {MicroOp::SBC8, &temp8_}),
				/* 0xdf RST 18h */	RST(),
				/* 0xe0 RET PO */	RET(TestPO),							/* 0xe1 POP HL */	Program(POP(index)),
				/* 0xe2 JP PO */	JP(TestPO),								/* 0xe3 EX (SP), HL */XX,
				/* 0xe4 CALL PO */	CALL(TestPO),							/* 0xe5 PUSH HL */	Program(WAIT(1), PUSH(index)),
				/* 0xe6 AND n */	Program(FETCH(temp8_, pc_), {MicroOp::And, &temp8_}),
				/* 0xe7 RST 20h */	RST(),
				/* 0xe8 RET PE */	RET(TestPE),							/* 0xe9 JP (HL) */	Program({MicroOp::Move16, &index.full, &pc_.full}),
				/* 0xea JP PE */	JP(TestPE),								/* 0xeb EX DE, HL */Program({MicroOp::ExDEHL}),
				/* 0xec CALL PE */	CALL(TestPE),							/* 0xed [ED page] */Program({MicroOp::SetInstructionPage, &ed_page_}),
				/* 0xee XOR n */	Program(FETCH(temp8_, pc_), {MicroOp::Xor, &temp8_}),
				/* 0xef RST 28h */	RST(),
				/* 0xf0 RET p */	RET(TestP),								/* 0xf1 POP AF */	Program(POP(temp16_), {MicroOp::DisassembleAF}),
				/* 0xf2 JP P */		JP(TestP),								/* 0xf3 DI */		Program({MicroOp::DI}),
				/* 0xf4 CALL P */	CALL(TestP),							/* 0xf5 PUSH AF */	Program(WAIT(1), {MicroOp::AssembleAF}, PUSH(temp16_)),
				/* 0xf6 OR n */		Program(FETCH(temp8_, pc_), {MicroOp::Or, &temp8_}),
				/* 0xf7 RST 30h */	RST(),
				/* 0xf8 RET M */	RET(TestM),								/* 0xf9 LD SP, HL */Program(WAIT(2), {MicroOp::Move16, &index.full, &sp_.full}),
				/* 0xfa JP M */		JP(TestM),								/* 0xfb EI */		Program({MicroOp::EI}),
				/* 0xfc CALL M */	CALL(TestM),							/* 0xfd [FD page] */Program({MicroOp::SetInstructionPage, &fd_page_}),
				/* 0xfe CP n */		Program(FETCH(temp8_, pc_), {MicroOp::CP8, &temp8_}),
				/* 0xff RST 38h */	RST(),
			};
			assemble_cb_page(cb_page, index, add_offsets);
			assemble_page(target, base_program_table, add_offsets);
		}

		void assemble_fetch_decode_execute() {
			// TODO: this can't legitimately be static and contain references to this via pc_ and operation_;
			// make it something else that is built at instance construction.
			const MicroOp fetch_decode_execute[] = {
				{ MicroOp::BusOperation, nullptr, nullptr, {ReadOpcode, 4, &pc_.full, &operation_}},
				{ MicroOp::DecodeOperation },
				{ MicroOp::MoveToNextProgram }
			};
			fetch_decode_execute_ = new MicroOp[3];
			fetch_decode_execute_[0] = fetch_decode_execute[0];
			fetch_decode_execute_[1] = fetch_decode_execute[1];
			fetch_decode_execute_[2] = fetch_decode_execute[2];
		}

		void decode_operation(uint8_t operation) {
			if(current_instruction_page_->instructions[operation]->type == MicroOp::None) {
				uint8_t page = 0x00;
				if(current_instruction_page_ == &ed_page_) page = 0xed;
				if(current_instruction_page_ == &fd_page_) page = 0xfd;
				printf("Unknown Z80 operation %02x %02x!!!\n", page, operation);
			} else schedule_program(current_instruction_page_->instructions[operation]);
		}

	public:
		Processor() : MicroOpScheduler() {
			assemble_base_page(base_page_, hl_, false, cb_page_);
			assemble_base_page(dd_page_, ix_, true, ddcb_page_);
			assemble_base_page(fd_page_, iy_, true, fdcb_page_);
			assemble_ed_page(ed_page_);
			assemble_fetch_decode_execute();

			fdcb_page_.increments_r = false;
			ddcb_page_.increments_r = false;
		}
		~Processor() {
			delete[] fetch_decode_execute_;
		}

		/*!
			Runs the Z80 for a supplied number of cycles.

			@discussion Subclasses must implement @c perform_machine_cycle(MachineCycle *cycle) .

			If it is a read operation then @c value will be seeded with the value 0xff.

			@param number_of_cycles The number of cycles to run the Z80 for.
		*/
		void run_for_cycles(int number_of_cycles) {

#define checkSchedule() \
	if(!scheduled_programs_[schedule_programs_read_pointer_]) {\
		current_instruction_page_ = &base_page_;\
		schedule_program(fetch_decode_execute_);\
	}

			number_of_cycles_ += number_of_cycles;
			checkSchedule();

			while(1) {
				const MicroOp *operation = &scheduled_programs_[schedule_programs_read_pointer_][schedule_program_program_counter_];
				schedule_program_program_counter_++;

#define set_parity(v)	\
	parity_overflow_flag_ = v^1;\
	parity_overflow_flag_ ^= parity_overflow_flag_ >> 4;\
	parity_overflow_flag_ ^= parity_overflow_flag_ << 2;\
	parity_overflow_flag_ ^= parity_overflow_flag_ >> 1;\
	parity_overflow_flag_ &= Flag::Parity;

				switch(operation->type) {
					case MicroOp::BusOperation:
						if(number_of_cycles_ < operation->machine_cycle.length) { schedule_program_program_counter_--; return; }
						number_of_cycles_ -= operation->machine_cycle.length;
						number_of_cycles_ -= static_cast<T *>(this)->perform_machine_cycle(&operation->machine_cycle);
					break;
					case MicroOp::MoveToNextProgram:
						move_to_next_program();
						checkSchedule();
					break;
					case MicroOp::DecodeOperation:
						if(current_instruction_page_->increments_r) r_ = (r_ & 0x80) | ((r_ + 1) & 0x7f);
						pc_.full++;
						decode_operation(operation_);
					break;

					case MicroOp::Increment16:			(*(uint16_t *)operation->source)++;											break;
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

					case MicroOp::And:
						a_ &= *(uint8_t *)operation->source;
						half_carry_flag_ = Flag::HalfCarry;
						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = a_;
						parity_overflow_flag_ = 0;
						set_parity(a_);
					break;

					case MicroOp::Or:
						a_ |= *(uint8_t *)operation->source;
						half_carry_flag_ = 0;
						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = a_;
						parity_overflow_flag_ = 0;
						set_parity(a_);
					break;

					case MicroOp::Xor:
						a_ ^= *(uint8_t *)operation->source;
						half_carry_flag_ = 0;
						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = a_;
						parity_overflow_flag_ = 0;
						set_parity(a_);
					break;

					case MicroOp::CPL:
						a_ ^= 0xff;
						subtract_flag_ = Flag::Subtract;
						half_carry_flag_ = Flag::HalfCarry;
						bit5_result_ = bit3_result_ = a_;
					break;

					case MicroOp::CCF:
						half_carry_flag_ = carry_flag_ << 4;
						carry_flag_ ^= Flag::Carry;
						subtract_flag_ = 0;
						bit5_result_ = bit3_result_ = a_;
					break;

					case MicroOp::SCF:
						carry_flag_ = Flag::Carry;
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
						bit5_result_ = bit3_result_ = a_;
					break;

#pragma mark - Flow control

					case MicroOp::DJNZ:
						bc_.bytes.high--;
						if(!bc_.bytes.high) {
							move_to_next_program();
							checkSchedule();
						}
					break;

					case MicroOp::CalculateRSTDestination:
						temp16_.full = operation_ & 0x38;
					break;

#pragma mark - 8-bit arithmetic

					case MicroOp::CP8: {
						uint8_t value = *(uint8_t *)operation->source;
						int result = a_ - value;
						int halfResult = (a_&0xf) - (value&0xf);

						// overflow for a subtraction is when the signs were originally
						// different and the result is different again
						int overflow = (value^a_) & (result^a_);

						sign_result_ =			// set sign and zero
						zero_result_ = (uint8_t)result;
						bit3_result_ = bit5_result_ = value;		// set the 5 and 3 flags, which come
																	// from the operand atypically
						carry_flag_ = (result >> 8) & Flag::Carry;
						half_carry_flag_ = halfResult & Flag::HalfCarry;
						parity_overflow_flag_ =	(overflow&0x80) >> 5;
						subtract_flag_ = Flag::Subtract;
					} break;

					case MicroOp::SUB8: {
						uint8_t value = *(uint8_t *)operation->source;
						int result = a_ - value;
						int halfResult = (a_&0xf) - (value&0xf);

						// overflow for a subtraction is when the signs were originally
						// different and the result is different again
						int overflow = (value^a_) & (result^a_);

						a_ = (uint8_t)result;

						sign_result_ = zero_result_ =
						bit5_result_ = bit3_result_ = (uint8_t)result;
						carry_flag_ = (result >> 8) & Flag::Carry;
						half_carry_flag_ = halfResult & Flag::HalfCarry;
						parity_overflow_flag_ =	(overflow&0x80) >> 5;
						subtract_flag_ = Flag::Subtract;
					} break;

					case MicroOp::SBC8: {
						uint8_t value = *(uint8_t *)operation->source;
						int result = a_ - value - carry_flag_;
						int halfResult = (a_&0xf) - (value&0xf) - carry_flag_;

						// overflow for a subtraction is when the signs were originally
						// different and the result is different again
						int overflow = (value^a_) & (result^a_);

						a_ = (uint8_t)result;

						sign_result_ = zero_result_ =
						bit5_result_ = bit3_result_ = (uint8_t)result;
						carry_flag_ = (result >> 8) & Flag::Carry;
						half_carry_flag_ = halfResult & Flag::HalfCarry;
						parity_overflow_flag_ =	(overflow&0x80) >> 5;
						subtract_flag_ = Flag::Subtract;
					} break;

					case MicroOp::ADD8: {
						uint8_t value = *(uint8_t *)operation->source;
						int result = a_ + value;
						int halfResult = (a_&0xf) + (value&0xf);

						// overflow for addition is when the signs were originally
						// the same and the result is different
						int overflow = ~(value^a_) & (result^a_);

						a_ = (uint8_t)result;

						sign_result_ = zero_result_ =
						bit3_result_ = bit5_result_ = (uint8_t)result;
						carry_flag_	 = (result >> 8) & Flag::Carry;
						half_carry_flag_ = halfResult & Flag::HalfCarry;
						parity_overflow_flag_ = (overflow&0x80) >> 5;
						subtract_flag_ = 0;
					} break;

					case MicroOp::ADC8: {
						uint8_t value = *(uint8_t *)operation->source;
						int result = a_ + value + carry_flag_;
						int halfResult = (a_&0xf) + (value&0xf) + carry_flag_;

						// overflow for addition is when the signs were originally
						// the same and the result is different
						int overflow = ~(value^a_) & (result^a_);

						a_ = (uint8_t)result;

						sign_result_ = zero_result_ =
						bit5_result_ = bit3_result_ = (uint8_t)result;
						carry_flag_ = (result >> 8) & Flag::Carry;
						half_carry_flag_ = halfResult & Flag::HalfCarry;
						parity_overflow_flag_ = (overflow&0x80) >> 5;
						subtract_flag_ = 0;
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
						bit5_result_ = bit3_result_ = sign_result_ = zero_result_ = (uint8_t)result;
						half_carry_flag_ = half_result & Flag::HalfCarry;
						parity_overflow_flag_ = (overflow >> 5)&Flag::Overflow;
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
						bit5_result_ = bit3_result_ = sign_result_ = zero_result_ = (uint8_t)result;
						half_carry_flag_ = half_result & Flag::HalfCarry;
						parity_overflow_flag_ = (overflow >> 5)&Flag::Overflow;
						subtract_flag_ = Flag::Subtract;
					} break;

					case MicroOp::DAA: {
						int lowNibble = a_ & 0xf;
						int highNibble = a_ >> 4;
						int amountToAdd = 0;

						if(carry_flag_)
						{
							amountToAdd = (lowNibble > 0x9 || half_carry_flag_) ? 0x66 : 0x60;
						}
						else
						{
							if(half_carry_flag_)
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

						if(!carry_flag_)
						{
							if(lowNibble > 0x9)
							{
								if(highNibble > 0x8) carry_flag_ = Flag::Carry;
							}
							else
							{
								if(highNibble > 0x9) carry_flag_ = Flag::Carry;
							}
						}

						if(subtract_flag_)
						{
							a_ -= amountToAdd;
							half_carry_flag_ = (half_carry_flag_ && lowNibble < 0x6) ? Flag::HalfCarry : 0;
						}
						else
						{
							a_ += amountToAdd;
							half_carry_flag_ = (lowNibble > 0x9) ? Flag::HalfCarry : 0;
						}

						sign_result_ = zero_result_ = bit3_result_ = bit5_result_ = a_;

						set_parity(a_);
					} break;

#pragma mark - 16-bit arithmetic

					case MicroOp::ADD16: {
						uint16_t sourceValue = *(uint16_t *)operation->source;
						uint16_t destinationValue = *(uint16_t *)operation->destination;
						int result = sourceValue + destinationValue;
						int halfResult = (sourceValue&0xfff) + (destinationValue&0xfff);

						bit3_result_ = bit5_result_ = (uint8_t)(result >> 8);
						carry_flag_ = (result >> 16) & Flag::Carry;
						half_carry_flag_ = (halfResult >> 8) & Flag::HalfCarry;
						subtract_flag_ = 0;

						*(uint16_t *)operation->destination = (uint16_t)result;
					} break;

					case MicroOp::ADC16: {
						uint16_t sourceValue = *(uint16_t *)operation->source;
						uint16_t destinationValue = *(uint16_t *)operation->destination;
						int result = sourceValue + destinationValue + carry_flag_;
						int halfResult = (sourceValue&0xfff) + (destinationValue&0xfff) + carry_flag_;

						int overflow = (result ^ destinationValue) & ~(destinationValue ^ sourceValue);

						bit5_result_	=
						bit3_result_	=
						sign_result_	= (uint8_t)(result >> 8);
						zero_result_	= (uint8_t)(result | sign_result_);
						subtract_flag_	= 0;
						carry_flag_		= result >> 16;
						half_carry_flag_ = (halfResult >> 8) & Flag::HalfCarry;
						parity_overflow_flag_ = (overflow & 0x8000) >> 13;

						*(uint16_t *)operation->destination = (uint16_t)result;
					} break;

					case MicroOp::SBC16: {
						uint16_t sourceValue = *(uint16_t *)operation->source;
						uint16_t destinationValue = *(uint16_t *)operation->destination;
						int result = destinationValue - sourceValue - carry_flag_;
						int halfResult = (destinationValue&0xfff) - (sourceValue&0xfff) - carry_flag_;

						// subtraction, so parity rules are:
						// signs of operands were different, 
						// sign of result is different
						int overflow = (result ^ destinationValue) & (sourceValue ^ destinationValue);

						bit5_result_	=
						bit3_result_	=
						sign_result_	= (uint8_t)(result >> 8);
						zero_result_	= (uint8_t)(result | sign_result_);
						subtract_flag_	= Flag::Subtract;
						carry_flag_		= result >> 16;
						half_carry_flag_ = (halfResult >> 8) & Flag::HalfCarry;
						parity_overflow_flag_ = (overflow & 0x8000) >> 13;

						*(uint16_t *)operation->destination = (uint16_t)result;
					} break;

					case MicroOp::TestNZ:	if(!zero_result_)			{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestZ:	if(zero_result_)			{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestNC:	if(carry_flag_)				{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestC:	if(!carry_flag_)			{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestPO:	if(parity_overflow_flag_)	{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestPE:	if(!parity_overflow_flag_)	{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestP:	if(sign_result_ & 0x80)		{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestM:	if(!(sign_result_ & 0x80))	{ move_to_next_program(); checkSchedule(); }		break;

					case MicroOp::ExDEHL: {
						uint16_t temp = de_.full;
						de_.full = hl_.full;
						hl_.full = temp;
					} break;

					case MicroOp::ExAFAFDash: {
						uint8_t a = a_;
						uint8_t f = get_flags();
						set_flags(afDash_.bytes.low);
						a_ = afDash_.bytes.high;
						afDash_.bytes.high = a;
						afDash_.bytes.low = f;
					} break;

#pragma mark - Repetition

					case MicroOp::LDIR: {
						bc_.full--;
						de_.full++;
						hl_.full++;

						bit3_result_ = bit5_result_ = a_ + temp8_;
						subtract_flag_ = 0;
						half_carry_flag_ = 0;

						if(bc_.full) {
							parity_overflow_flag_ = Flag::Parity;
							pc_.full -= 2;
						} else {
							parity_overflow_flag_ = 0;
							move_to_next_program();
							checkSchedule();
						}
					} break;

#pragma mark - Bit Manipulation

					case MicroOp::BIT: {
						uint8_t result = *(uint8_t *)operation->source & (1 << ((operation_ >> 3)&7));

						sign_result_ = zero_result_ = result;
						bit3_result_ = bit5_result_ = *(uint8_t *)operation->source;	// This is a divergence between FUSE and The Undocumented Z80 Documented.
						half_carry_flag_ = Flag::HalfCarry;
						subtract_flag_ = 0;
						parity_overflow_flag_ = result ? 0 : Flag::Parity;
					} break;

					case MicroOp::RES:
						*(uint8_t *)operation->source &= ~(1 << ((operation_ >> 3)&7));
					break;

					case MicroOp::SET:
						*(uint8_t *)operation->source |= (1 << ((operation_ >> 3)&7));
					break;

#pragma mark - Rotation and shifting

					case MicroOp::RLA: {
						uint8_t new_carry = a_ >> 7;
						a_ = (uint8_t)((a_ << 1) | carry_flag_);
						bit3_result_ = bit5_result_ = a_;
						carry_flag_ = new_carry;
						subtract_flag_ = half_carry_flag_ = 0;
					} break;

					case MicroOp::RLCA: {
						uint8_t new_carry = a_ >> 7;
						a_ = (uint8_t)((a_ << 1) | new_carry);
						bit3_result_ = bit5_result_ = a_;
						carry_flag_ = new_carry;
						subtract_flag_ = half_carry_flag_ = 0;
					} break;

					case MicroOp::RRA: {
						uint8_t newCarry = a_ & 1;
						a_ = (uint8_t)((a_ >> 1) | (carry_flag_ << 7));
						bit3_result_ = bit5_result_ = a_;
						carry_flag_ = newCarry;
						subtract_flag_ = half_carry_flag_ = 0;
					} break;

					case MicroOp::RRCA: {
						uint8_t newCarry = a_ & 1;
						a_ = (uint8_t)((a_ >> 1) | (newCarry << 7));
						bit5_result_ = bit3_result_ = a_;
						carry_flag_ = newCarry;
						subtract_flag_ = half_carry_flag_ = 0;
					} break;

					case MicroOp::RLC: {
						carry_flag_ = *(uint8_t *)operation->source >> 7;
						*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source << 1) | carry_flag_);

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

					case MicroOp::RRC: {
						carry_flag_ = *(uint8_t *)operation->source & 1;
						*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1) | (carry_flag_ << 7));

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

					case MicroOp::RL: {
						uint8_t next_carry = *(uint8_t *)operation->source >> 7;
						*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source << 1) | carry_flag_);
						carry_flag_ = next_carry;

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

					case MicroOp::RR: {
						uint8_t next_carry = *(uint8_t *)operation->source & 1;
						*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1) | (carry_flag_ << 7));
						carry_flag_ = next_carry;

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

					case MicroOp::SLA: {
						carry_flag_ = *(uint8_t *)operation->source >> 7;
						*(uint8_t *)operation->source = (uint8_t)(*(uint8_t *)operation->source << 1);

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

					case MicroOp::SRA: {
						carry_flag_ = *(uint8_t *)operation->source & 1;
						*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1) | (*(uint8_t *)operation->source & 0x80));

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

					case MicroOp::SLL: {
						carry_flag_ = *(uint8_t *)operation->source >> 7;
						*(uint8_t *)operation->source = (uint8_t)(*(uint8_t *)operation->source << 1) | 1;

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

					case MicroOp::SRL: {
						carry_flag_ = *(uint8_t *)operation->source & 1;
						*(uint8_t *)operation->source = (uint8_t)((*(uint8_t *)operation->source >> 1));

						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = *(uint8_t *)operation->source;
						set_parity(sign_result_);
						half_carry_flag_ = 0;
						subtract_flag_ = 0;
					} break;

#pragma mark - Interrupt state

					case MicroOp::EI:
						iff1_ = iff2_ = true;
					break;

					case MicroOp::DI:
						iff1_ = iff2_ = false;
					break;

#pragma mark - Internal bookkeeping

					case MicroOp::SetInstructionPage:
						schedule_program(fetch_decode_execute_);
						current_instruction_page_ = (InstructionPage *)operation->source;
//						printf("+ ");
					break;

					case MicroOp::CalculateIndexAddress:
						temp16_.full = *(uint16_t *)operation->source + (int8_t)temp8_;
					break;

					default:
//						printf("Unhandled Z80 operation %d\n", operation->type);
					return;
				}
#undef set_parity

			}
		}

		/*!
			Called to announce the end of a run_for_cycles period, allowing deferred work to take place.

			Users of the Z80 template may override this.
		*/
		void flush() {}

		int perform_machine_cycle(const MachineCycle *cycle) {
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
				(bit5_result_ & Flag::Bit5) |
				half_carry_flag_ |
				(bit3_result_ & Flag::Bit3) |
				parity_overflow_flag_ |
				subtract_flag_ |
				carry_flag_;
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
			bit5_result_			= flags;
			half_carry_flag_		= flags & Flag::HalfCarry;
			bit3_result_			= flags;
			parity_overflow_flag_	= flags & Flag::Parity;
			subtract_flag_			= flags & Flag::Subtract;
			carry_flag_				= flags & Flag::Carry;
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

				case Register::R:						return r_;
				case Register::I:						return i_;

				case Register::IFF1:					return iff1_ ? 1 : 0;
				case Register::IFF2:					return iff2_ ? 1 : 0;
				case Register::IM:						return interrupt_mode_;

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

				case Register::R:				r_ = (uint8_t)value;					break;
				case Register::I:				i_ = (uint8_t)value;					break;

				case Register::IFF1:			iff1_ = !!value;						break;
				case Register::IFF2:			iff2_ = !!value;						break;
				case Register::IM:				interrupt_mode_ = value % 2;			break;

				default: break;
			}
		}
};

}
}

#endif /* Z80_hpp */
