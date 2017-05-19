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
		Move16,
		Jump,

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
		uint8_t carry_flag_, sign_result_, bit5_result_, half_carry_flag_, bit3_result_, parity_overflow_flag_, subtract_flag_;

		int number_of_cycles_;
		const MicroOp **program_table_;

		uint8_t operation_;
		RegisterPair address_;

		void decode_base_operation(uint8_t operation) {
#define XX				{MicroOp::None, 0}
#define FETCH(x, y)		{MicroOp::BusOperation, nullptr, nullptr, {Read, 3, &y.full, &x}}
#define FETCH16(x, y)	FETCH(x.bytes.low, y), {MicroOp::Increment16, &y.full}, FETCH(x.bytes.high, y), {MicroOp::Increment16, &y.full}
#define FETCH16L(x, y)	FETCH(x.bytes.low, y), {MicroOp::Increment16, &y.full}, FETCH(x.bytes.high, y)
#define FETCH_LOW()		FETCH(address_.bytes.low, pc_)
#define FETCH_HIGH()	FETCH(address_.bytes.high, pc_)
#define WAIT(n)			{MicroOp::BusOperation, nullptr, nullptr, {Internal, n} }
#define Program(...)	{ __VA_ARGS__, {MicroOp::MoveToNextProgram} }

			static const MicroOp base_program_table[256][10] = {
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x00
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x08
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x10
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x18
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x20
				XX,			XX,			// 0x28
				Program(FETCH16(address_, pc_), FETCH16L(hl_, address_)),	/* 0x2a LD HL, (nn) */
				XX,			XX,			XX,			XX,			XX,
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x30
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x38
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x40
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x48
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x50
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x58
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x60
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x68
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x70
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x78
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x80
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x88
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x90
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0x98
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xa0
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xa8
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xb0
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xb8
				XX,	/* 0xc0 */
				XX,	/* 0xc1 */
				XX,	/* 0xc2 */
				Program(FETCH16L(address_, pc_), {MicroOp::Jump}),	/* 0xc3 JP nn */
				XX,	/* 0xc4 */
				XX,	/* 0xc5 */
				XX,	/* 0xc6 */
				XX,	/* 0xc7 */
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xc8
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xd0
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xd8
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xe0
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xe8
				XX,			XX,			XX,			XX,			XX,			XX,			XX,			XX,		// 0xf0
				XX,	/* 0xf8 */
				Program(WAIT(2), {MicroOp::Move16, &hl_.full, &sp_.full}),	/* 0xF9 LD SP, HL */
				XX,			XX,			XX,			XX,			XX,			XX,		// 0xf8
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
				{ MicroOp::BusOperation, 4, nullptr, nullptr, {ReadOpcode, &pc_.full, &operation_}},
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

					case MicroOp::Increment16:			(*((uint16_t *)operation->source))++;			break;
					case MicroOp::Jump:					pc_ = address_;									break;

					case MicroOp::Move16:				*(uint16_t *)operation->destination = *(uint16_t *)operation->source;			break;

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
				carry_flag_ |
				(sign_result_ & Flag::Sign) |
				(bit5_result_ & Flag::Bit5) |
				half_carry_flag_ |
				(bit3_result_ & Flag::Bit3) |
				parity_overflow_flag_ |
				subtract_flag_;
		}

		/*!
			Sets the flags register.

			@see set_flags

			@param flags The new value of the flags register.
		*/
		void set_flags(uint8_t flags) {
			carry_flag_				= flags & Flag::Carry;
			sign_result_			= flags;
			bit5_result_			= flags;
			half_carry_flag_		= flags & Flag::HalfCarry;
			bit3_result_			= flags;
			parity_overflow_flag_	= flags & Flag::Parity;
			subtract_flag_			= flags & Flag::Subtract;
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
