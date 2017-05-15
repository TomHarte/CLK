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
	ReadOpcode,
	Read, Write,
	Input, Output,
	Interrupt,
	BusRequest, BusAcknowledge,
	None
};

struct MicroOp {
	enum {
	} type;
	void *source;
	void *destination;
};

/*!
	@abstact An abstract base class for emulation of a 6502 processor via the curiously recurring template pattern/f-bounded polymorphism.

	@discussion Subclasses should implement @c perform_bus_operation(BusOperation operation, uint16_t address, uint8_t *value) in
	order to provide the bus on which the 6502 operates and @c synchronise(), which is called upon completion of a continuous run
	of cycles to allow a subclass to bring any on-demand activities up to date.

	Additional functionality can be provided by the host machine by providing a jam handler and inserting jam opcodes where appropriate;
	that will cause call outs when the program counter reaches those addresses. @c return_from_subroutine can be used to exit from a
	jammed state.
*/
template <class T> class Processor: public MicroOpScheduler<MicroOp> {
	private:

		RegisterPair bc_, de_, hl_, afDash_, bcDash_, hlDash_, ix_, iy_;
		uint8_t a, i, r;

		const MicroOp *scheduled_programs_[4];
		unsigned int schedule_programs_write_pointer_, schedule_programs_read_pointer_, schedule_program_program_counter_;
};

}
}

#endif /* Z80_hpp */
