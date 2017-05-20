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
	const BusOperation operation;
	const int length;
	const uint16_t *address;
	uint8_t *const value;
};

struct MicroOp {
	enum {
		BusOperation,
		DecodeOperation,
		MoveToNextProgram,

		Increment16,
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

		None
	} type;
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
		uint8_t sign_result_, zero_result_, bit5_result_, half_carry_flag_, bit3_result_, parity_overflow_flag_, subtract_flag_, carry_flag_;

		int number_of_cycles_;
		const MicroOp **program_table_;

		uint8_t operation_;
		RegisterPair address_, temporary_;

		void decode_base_operation(uint8_t operation) {
#define XX				{MicroOp::None, 0}

#define FETCH(x, y)		{MicroOp::BusOperation, nullptr, nullptr, {Read, 3, &y.full, &x}}, {MicroOp::Increment16, &y.full}
#define FETCHL(x, y)	{MicroOp::BusOperation, nullptr, nullptr, {Read, 3, &y.full, &x}}

#define STOREL(x, y)	{MicroOp::BusOperation, nullptr, nullptr, {Write, 3, &y.full, &x}}

#define FETCH16(x, y)	FETCH(x.bytes.low, y), FETCH(x.bytes.high, y)
#define FETCH16L(x, y)	FETCH(x.bytes.low, y), FETCHL(x.bytes.high, y)

#define PUSH(x)			{MicroOp::Decrement16, &sp_.full}, STOREL(x.bytes.high, sp_), {MicroOp::Decrement16, &sp_.full}, STOREL(x.bytes.low, sp_)
#define POP(x)			FETCHL(x.bytes.low, sp_), {MicroOp::Increment16, &sp_.full}, FETCHL(x.bytes.high, sp_), {MicroOp::Increment16, &sp_.full}

#define JP(cc)			Program(FETCH16(temporary_, pc_), {MicroOp::cc}, {MicroOp::Move16, &address_.full, &pc_.full})
#define LD(a, b)		Program({MicroOp::Move8, &b, &a})

#define LD_GROUP(r)	\
				LD(r, bc_.bytes.high),	LD(r, bc_.bytes.low),	LD(r, de_.bytes.high),		LD(r, de_.bytes.low),	\
				LD(r, hl_.bytes.high),	LD(r, hl_.bytes.low),	Program(FETCHL(r, hl_)),	LD(r, a_)

#define OP_GROUP(op)	\
				Program({MicroOp::op, &bc_.bytes.high}),	Program({MicroOp::op, &bc_.bytes.low}),	\
				Program({MicroOp::op, &de_.bytes.high}),	Program({MicroOp::op, &de_.bytes.low}),	\
				Program({MicroOp::op, &hl_.bytes.high}),	Program({MicroOp::op, &hl_.bytes.low}),	\
				Program(FETCHL(temporary_.bytes.low, hl_), {MicroOp::op, &temporary_.bytes.low}),	\
				Program({MicroOp::op, &a_})

#define WAIT(n)			{MicroOp::BusOperation, nullptr, nullptr, {Internal, n} }
#define Program(...)	{ __VA_ARGS__, {MicroOp::MoveToNextProgram} }


			static const MicroOp base_program_table[256][20] = {
				{ {MicroOp::MoveToNextProgram} },		/* 0x00 NOP */
				Program(FETCH16(bc_, pc_)),				/* 0x01 LD BC, nn */
				XX,	/* 0x02 LD (BC), A */
				Program(WAIT(2), {MicroOp::Increment16, &bc_.full}), /* 0x03 INC BC */
				XX,	/* 0x04 INC B */
				XX,	/* 0x05 DEC B */
				Program(FETCH(bc_.bytes.high, pc_)),	/* 0x06 LD B, n */
				XX,	/* 0x07 RLCA */
				XX,	/* 0x08 EX AF, AF' */
				XX,	/* 0x09 ADD HL, BC */
				Program(FETCHL(a_, bc_)),	/* 0x0a LD A, (BC) */
				Program(WAIT(2), {MicroOp::Decrement16, &bc_.full}),	/* 0x0b DEC BC */
				XX,	/* 0x0c INC C */
				XX,	/* 0x0d DEC C */
				Program(FETCH(bc_.bytes.low, pc_)),		/* 0x0e LD C, n */
				XX,	/* 0x0f RRCA */
				XX,	/* 0x10 DJNZ */
				Program(FETCH16(de_, pc_)),				/* 0x11 LD DE, nn */
				XX,	/* 0x12 LD (DE), A */
				Program(WAIT(2), {MicroOp::Increment16, &de_.full}), /* 0x13 INC DE */
				XX, /* 0x14 INC D */
				XX, /* 0x15 DEC D */
				Program(FETCH(de_.bytes.high, pc_)),	/* 0x16 LD D, n */
				XX, /* 0x17 RLA */
				XX,	/* 0x18 JR */
				XX,	/* 0x19 ADD HL, DE */
				Program(FETCHL(a_, de_)),	/* 0x1a LD A, (DE) */
				Program(WAIT(2), {MicroOp::Decrement16, &de_.full}),	/* 0x1b DEC DE */
				XX,	/* 0x1c INC E */
				XX,	/* 0x1d DEC E */
				Program(FETCH(de_.bytes.low, pc_)),		/* 0x1e LD E, n */
				XX,	/* 0x1f RRA */
				XX,	/* 0x20 JR NZ */
				Program(FETCH16(hl_, pc_)), /* 0x21 LD HL, nn */
				XX,	/* 0x22 LD (nn), HL */
				Program(WAIT(2), {MicroOp::Increment16, &hl_.full}),	/* 0x23 INC HL */
				XX,	/* 0x24 INC H */
				XX, /* 0x25 DEC H */
				Program(FETCH(hl_.bytes.high, pc_)),	/* 0x26 LD H, n */
				XX,	/* 0x27 DAA */
				XX,	/* 0x28 JR Z */
				XX,	/* 0x29 ADD HL, HL */
				Program(FETCH16(address_, pc_), FETCH16L(hl_, address_)),	/* 0x2a LD HL, (nn) */
				Program(WAIT(2), {MicroOp::Decrement16, &hl_.full}),	/* 0x2b DEC HL */
				XX,	/* 0x2c INC L */
				XX,	/* 0x2d DEC L */
				Program(FETCH(hl_.bytes.low, pc_)),	/* 0x2e LD L, n */
				XX, /* 0x2f CPL */
				XX,	/* 0x30 JR NC */
				Program(FETCH16(sp_, pc_)),	/* 0x31 LD SP, nn */
				XX,	/* 0x32 LD (nn), A */
				Program(WAIT(2), {MicroOp::Increment16, &sp_.full}), /* 0x33 INC SP */
				XX,	/* 0x34 INC (HL) */
				XX,	/* 0x35 DEC (HL) */
				XX,	/* 0x36 LD (HL), n */
				XX,	/* 0x37 SCF */
				XX,	/* 0x38 JR C */
				XX,	/* 0x39 ADD HL, SP */
				XX,	/* 0x3a LD A, (nn) */
				Program(WAIT(2), {MicroOp::Decrement16, &sp_.full}),	/* 0x3b DEC SP */
				XX,	/* 0x3c INC A */
				XX,	/* 0x3d DEC A */
				XX,	/* 0x3e LD A, n */
				XX,	/* 0x3f CCF */
				LD_GROUP(bc_.bytes.high),	/* 0x40 LD B, B;  0x41 LD B, C;	0x42 LD B, D;	0x43 LD B, E;	0x44 LD B, H;	0x45 LD B, L;	0x46 LD B, (HL);	0x47 LD B, A */
				LD_GROUP(bc_.bytes.low),	/* 0x48 LD C, B;  0x49 LD C, C;	0x4a LD C, D;	0x4b LD C, E;	0x4c LD C, H;	0x4d LD C, L;	0x4e LD C, (HL);	0x4f LD C, A */
				LD_GROUP(de_.bytes.high),	/* 0x50 LD D, B;  0x51 LD D, C;	0x52 LD D, D;	0x53 LD D, E;	0x54 LD D, H;	0x55 LD D, L;	0x56 LD D, (HL);	0x57 LD D, A */
				LD_GROUP(de_.bytes.low),	/* 0x58 LD E, B;  0x59 LD E, C;	0x5a LD E, D;	0x5b LD E, E;	0x5c LD E, H;	0x5d LD E, L;	0x5e LD E, (HL);	0x5f LD E, A */
				LD_GROUP(hl_.bytes.high),	/* 0x60 LD H, B;  0x61 LD H, C;	0x62 LD H, D;	0x63 LD H, E;	0x64 LD H, H;	0x65 LD H, L;	0x66 LD H, (HL);	0x67 LD H, A */
				LD_GROUP(hl_.bytes.low),	/* 0x68 LD L, B;  0x69 LD L, C;	0x6a LD L, D;	0x6b LD L, E;	0x6c LD L, H;	0x6d LD H, L;	0x6e LD L, (HL);	0x6f LD L, A */
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x70
				LD_GROUP(a_),				/* 0x78 LD A, B;  0x79 LD A, C;	0x7a LD A, D;	0x7b LD A, E;	0x7c LD A, H;	0x7d LD A, L;	0x7e LD A, (HL);	0x7f LD A, A */
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x80
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x88
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x90
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x98
				OP_GROUP(And),	/* 0xa0 AND B;	0xa1 AND C;	0xa2 AND D;	0xa3 AND E;	0xa4 AND H;	0xa5 AND L;	0xa6 AND (HL);	0xa7 AND A */
				OP_GROUP(Xor),	/* 0xa8 XOR B;	0xa9 XOR C;	0xaa XOR D;	0xab XOR E;	0xac XOR H;	0xad XOR L;	0xae XOR (HL);	0xaf XOR A */
				OP_GROUP(Or),	/* 0xb0 OR B;	0xb1 OR C;	0xb2 OR D;	0xb3 OR E;	0xb4 OR H;	0xb5 OR L;	0xb6 OR (HL);	0xb7 OR A */
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xb8
				XX,	/* 0xc0 RET NZ */
				Program(POP(bc_)),	/* 0xc1 POP BC */
				JP(TestNZ),	/* 0xc2 JP NZ */
				Program(FETCH16L(address_, pc_), {MicroOp::Move16, &address_.full, &pc_.full}),	/* 0xc3 JP nn */
				XX,	/* 0xc4 CALL NZ */
				Program(WAIT(1), PUSH(bc_)),	/* 0xc5 PUSH BC */
				XX,	/* 0xc6 ADD A, n */
				XX,	/* 0xc7 RST 00h */
				XX,	/* 0xc8 RET Z */
				Program(POP(pc_)),	/* 0xc9 RET */
				JP(TestZ),	/* 0xca JP Z */
				XX,	/* 0xcb [CB page] */
				XX,	/* 0xcc CALL Z */
				Program(FETCH16(address_, pc_), WAIT(1), PUSH(pc_), {MicroOp::Move16, &address_.full, &pc_.full}),	/* 0xcd CALL */
				XX,	/* 0xce ADC A, n */
				XX,	/* 0xcf RST 08h */
				XX,	/* 0xd0 RET NC */
				Program(POP(de_)),	/* 0xd1 POP DE */
				JP(TestNC),	/* 0xd2 JP NC */
				XX,	/* 0xd3 OUT (n), A */
				XX,	/* 0xd4 CALL NC */
				Program(WAIT(1), PUSH(de_)),	/* 0xd5 PUSH DE */
				XX,	/* 0xd6 SUB n */
				XX,	/* 0xd7 RST 10h */
				XX,	/* 0xd8 RET C */
				XX,	/* 0xd9 EXX */
				JP(TestC),	/* 0xda JP C */
				XX,	/* 0xdb IN A, (n) */
				XX,	/* 0xdc CALL C */
				XX,	/* 0xdd [DD page] */
				XX,	/* 0xde SBC A, n */
				XX,	/* 0xdf RST 18h */
				XX,	/* 0xe0 RET PO */
				Program(POP(hl_)),	/* 0xe1 POP HL */
				JP(TestPO),	/* 0xe2 JP PO */
				XX,	/* 0xe3 EX (SP), HL */
				XX,	/* 0xe4 CALL PO */
				Program(WAIT(1), PUSH(hl_)),	/* 0xe5 PUSH HL */
				XX,	/* 0xe6 AND n */
				XX,	/* 0xe7 RST 20h */
				XX,	/* 0xe8 RET PE */
				XX,	/* 0xe9 JP (HL) */
				JP(TestPE),	/* 0xea JP PE */
				XX,	/* 0xeb EX DE, HL */
				XX,	/* 0xec CALL PE */
				XX,	/* 0xed [ED page] */
				XX,	/* 0xee XOR n */
				XX,	/* 0xef RST 28h */
				XX,	/* 0xf0 RET p */
				Program(POP(temporary_), {MicroOp::DisassembleAF}),	/* 0xf1 POP AF */
				JP(TestP),	/* 0xf2 JP P */
				XX,	/* 0xf3 DI */
				XX,	/* 0xf4 CALL P */
				Program(WAIT(1), {MicroOp::AssembleAF}, PUSH(temporary_)),	/* 0xf5 PUSH AF */
				XX,	/* 0xf6 OR n */
				XX,	/* 0xf7 RST 30h */
				XX,	/* 0xf8 RET M */
				Program(WAIT(2), {MicroOp::Move16, &hl_.full, &sp_.full}),	/* 0xf9 LD SP, HL */
				JP(TestM),	/* 0xfa JP M */
				XX,	/* 0xfb EI */
				XX,	/* 0xfc CALL M */
				XX,	/* 0xfd [FD page] */
				XX,	/* 0xfe CP n */
				XX,	/* 0xff RST 38h */
			};
			if(base_program_table[operation][0].type == MicroOp::None) {
				printf("Unknown Z80 operation %02x!!!\n", operation);
			}
			schedule_program(base_program_table[operation]);
//			program_table_ = base_program_table;
		}

	public:
		Processor() {
//			set_base_program_table();
		}

		/*!
			Runs the Z80 for a supplied number of cycles.

			@discussion Subclasses must implement @c perform_machine_cycle(MachineCycle *cycle) .

			If it is a read operation then @c value will be seeded with the value 0xff.

			@param number_of_cycles The number of cycles to run the Z80 for.
		*/
		void run_for_cycles(int number_of_cycles) {
			static const MicroOp fetch_decode_execute[] = {
				{ MicroOp::BusOperation, nullptr, nullptr, {ReadOpcode, 4, &pc_.full, &operation_}},
				{ MicroOp::DecodeOperation },
				{ MicroOp::MoveToNextProgram }
			};

#define checkSchedule() \
	if(!scheduled_programs_[schedule_programs_read_pointer_]) {\
		schedule_program(fetch_decode_execute);\
	}

			number_of_cycles_ += number_of_cycles;
			checkSchedule();

			while(1) {
				const MicroOp *operation = &scheduled_programs_[schedule_programs_read_pointer_][schedule_program_program_counter_];
				schedule_program_program_counter_++;

				switch(operation->type) {
					case MicroOp::BusOperation:
						if(number_of_cycles_ < operation->machine_cycle.length) {
							return;
						}
						static_cast<T *>(this)->perform_machine_cycle(&operation->machine_cycle);
					break;
					case MicroOp::MoveToNextProgram:
						move_to_next_program();
						checkSchedule();
					break;
					case MicroOp::DecodeOperation:
						pc_.full++;
						decode_base_operation(operation_);
					break;

					case MicroOp::Increment16:			(*(uint16_t *)operation->source)++;											break;
					case MicroOp::Decrement16:			(*(uint16_t *)operation->source)--;											break;
					case MicroOp::Move8:				*(uint8_t *)operation->destination = *(uint8_t *)operation->source;			break;
					case MicroOp::Move16:				*(uint16_t *)operation->destination = *(uint16_t *)operation->source;		break;

					case MicroOp::AssembleAF:
						temporary_.bytes.high = a_;
						temporary_.bytes.low = get_flags();
					break;
					case MicroOp::DisassembleAF:
						a_ = temporary_.bytes.high;
						set_flags(temporary_.bytes.low);
					break;

#define set_parity(v)	\
	parity_overflow_flag_ = v^1;\
	parity_overflow_flag_ ^= parity_overflow_flag_ >> 4;\
	parity_overflow_flag_ ^= parity_overflow_flag_ << 2;\
	parity_overflow_flag_ ^= parity_overflow_flag_ >> 1;\
	parity_overflow_flag_ &= Flag::Parity;

					case MicroOp::And:
						a_ &= *(uint8_t *)operation->source;
						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = a_;
						parity_overflow_flag_ = 0;
						set_parity(a_);
					break;

					case MicroOp::Or:
						a_ |= *(uint8_t *)operation->source;
						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = a_;
						parity_overflow_flag_ = 0;
						set_parity(a_);
					break;

					case MicroOp::Xor:
						a_ ^= *(uint8_t *)operation->source;
						sign_result_ = zero_result_ = bit5_result_ = bit3_result_ = a_;
						parity_overflow_flag_ = 0;
						set_parity(a_);
					break;

#undef set_parity

					case MicroOp::TestNZ:	if(!zero_result_)			{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestZ:	if(zero_result_)			{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestNC:	if(carry_flag_)				{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestC:	if(!carry_flag_)			{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestPO:	if(parity_overflow_flag_)	{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestPE:	if(!parity_overflow_flag_)	{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestP:	if(sign_result_ & 0x80)		{ move_to_next_program(); checkSchedule(); }		break;
					case MicroOp::TestM:	if(!(sign_result_ & 0x80))	{ move_to_next_program(); checkSchedule(); }		break;

					default:
						printf("Unhandled Z80 operation %d\n", operation->type);
					return;
				}
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
			return
				(sign_result_ & Flag::Sign) |
				(zero_result_ ? 0 : Flag::Zero) |
				(bit5_result_ & Flag::Bit5) |
				half_carry_flag_ |
				(bit3_result_ & Flag::Bit3) |
				parity_overflow_flag_ |
				subtract_flag_ |
				carry_flag_;
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

				default: break;
			}
		}
};

}
}

#endif /* Z80_hpp */
