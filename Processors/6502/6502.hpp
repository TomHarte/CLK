//
//  6502.hpp
//  CLK
//
//  Created by Thomas Harte on 09/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#ifndef MOS6502_cpp
#define MOS6502_cpp

#include <cassert>
#include <cstdio>
#include <cstdint>

#include "../RegisterSizes.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace CPU {
namespace MOS6502 {

/*
	The list of registers that can be accessed via @c set_value_of_register and @c set_value_of_register.
*/
enum Register {
	LastOperationAddress,
	ProgramCounter,
	StackPointer,
	Flags,
	A,
	X,
	Y
};

/*
	The list of 6502 variants supported by this implementation.
*/
enum Personality {
	PNES6502,			// the NES's 6502, which is like a 6502 but lacks decimal mode (though it retains the decimal flag)
	P6502,				// the original [NMOS] 6502, replete with various undocumented instructions
	PSynertek65C02,		// a 6502 extended with BRA, P[H/L][X/Y], STZ, TRB, TSB and the (zp) addressing mode and a few other additions
	PRockwell65C02,		// like the Synertek, but with BBR, BBS, RMB and SMB
	PWDC65C02,			// like the Rockwell, but with STP and WAI
};

#define has_decimal_mode(p)	((p) >= Personality::P6502)
#define is_65c02(p)			((p) >= Personality::PSynertek65C02)
#define has_bbrbbsrmbsmb(p)	((p) >= Personality::PRockwell65C02)
#define has_stpwai(p)		((p) >= Personality::PWDC65C02)

/*
	Flags as defined on the 6502; can be used to decode the result of @c get_value_of_register(Flags) or to form a value for
	the corresponding set.
*/
enum Flag: uint8_t {
	Sign		= 0x80,
	Overflow	= 0x40,
	Always		= 0x20,
	Break		= 0x10,
	Decimal		= 0x08,
	Interrupt	= 0x04,
	Zero		= 0x02,
	Carry		= 0x01
};

/*!
	Subclasses will be given the task of performing bus operations, allowing them to provide whatever interface they like
	between a 6502 and the rest of the system. @c BusOperation lists the types of bus operation that may be requested.

	@c None is reserved for internal use. It will never be requested from a subclass. It is safe always to use the
	isReadOperation macro to make a binary choice between reading and writing.
*/
enum BusOperation {
	Read, ReadOpcode, Write, Ready, None
};

/*!
	Evaluates to `true` if the operation is a read; `false` if it is a write.
*/
#define isReadOperation(v)	(v == CPU::MOS6502::BusOperation::Read || v == CPU::MOS6502::BusOperation::ReadOpcode)

/*!
	An opcode that is guaranteed to cause the CPU to jam.
*/
extern const uint8_t JamOpcode;

/*!
	A class providing empty implementations of the methods a 6502 uses to access the bus. To wire the 6502 to a bus,
	machines should subclass BusHandler and then declare a realisation of the 6502 template, suplying their bus
	handler.
*/
class BusHandler {
	public:
		/*!
			Announces that the 6502 has performed the cycle defined by operation, address and value. On the 6502,
			all bus cycles take one clock cycle so the amoutn of time advanced is implicit.

			@param operation The type of bus cycle: read, read opcode (i.e. read, with sync active),
			write or ready.
			@param address The value of the address bus during this bus cycle.
			@param value If this is a cycle that puts a value onto the data bus, *value is that value. If this is
			a cycle that reads the bus, the bus handler should write a value to *value. Writing to *value during
			a read cycle will produce undefined behaviour.

			@returns The number of cycles that passed in objective time while this 6502 bus cycle was ongoing.
			On an archetypal machine this will be Cycles(1) but some architectures may choose not to clock the 6502
			during some periods; one way to simulate that is to have the bus handler return a number other than
			Cycles(1) to describe lengthened bus cycles.
		*/
		Cycles perform_bus_operation(CPU::MOS6502::BusOperation operation, uint16_t address, uint8_t *value) {
			return Cycles(1);
		}

		/*!
			Announces completion of all the cycles supplied to a .run_for request on the 6502. Intended to allow
			bus handlers to perform any deferred output work.
		*/
		void flush() {}
};

#include "Implementation/6502Storage.hpp"

/*!
	A base class from which the 6502 descends; separated for implementation reasons only.
*/
class ProcessorBase: public ProcessorStorage {
	public:
		ProcessorBase(Personality personality) : ProcessorStorage(personality) {}

		/*!
			Gets the value of a register.

			@see set_value_of_register

			@param r The register to set.
			@returns The value of the register. 8-bit registers will be returned as unsigned.
		*/
		uint16_t get_value_of_register(Register r) const;

		/*!
			Sets the value of a register.

			@see get_value_of_register

			@param r The register to set.
			@param value The value to set. If the register is only 8 bit, the value will be truncated.
		*/
		void set_value_of_register(Register r, uint16_t value);

		/*!
			Sets the current level of the RST line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_reset_line(bool active);

		/*!
			Gets whether the 6502 would reset at the next opportunity.

			@returns @c true if the line is logically active; @c false otherwise.
		*/
		inline bool get_is_resetting() const;

		/*!
			This emulation automatically sets itself up in power-on state at creation, which has the effect of triggering a
			reset at the first opportunity. Use @c set_power_on to disable that behaviour.
		*/
		inline void set_power_on(bool active);

		/*!
			Sets the current level of the IRQ line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_irq_line(bool active);

		/*!
			Sets the current level of the set overflow line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		inline void set_overflow_line(bool active);

		/*!
			Sets the current level of the NMI line.

			@param active `true` if the line is logically active; `false` otherwise.
		*/
		inline void set_nmi_line(bool active);

		/*!
			Queries whether the 6502 is now 'jammed'; i.e. has entered an invalid state
			such that it will not of itself perform any more meaningful processing.

			@returns @c true if the 6502 is jammed; @c false otherwise.
		*/
		bool is_jammed() const;
};

/*!
	@abstact Template providing emulation of a 6502 processor.

	@discussion Users should provide as the first template parameter a subclass of CPU::MOS6502::BusHandler; the 6502
	will announce its cycle-by-cycle activity via the bus handler, which is responsible for marrying it to a bus. They
	can also nominate whether the processor includes support for the ready line. Declining to support the ready line
	can produce a minor runtime performance improvement.
*/
template <Personality personality, typename T, bool uses_ready_line> class Processor: public ProcessorBase {
	public:
		/*!
			Constructs an instance of the 6502 that will use @c bus_handler for all bus communications.
		*/
		Processor(T &bus_handler) : ProcessorBase(personality), bus_handler_(bus_handler) {}

		/*!
			Runs the 6502 for a supplied number of cycles.

			@param cycles The number of cycles to run the 6502 for.
		*/
		void run_for(const Cycles cycles);

		/*!
			Sets the current level of the RDY line.

			@param active @c true if the line is logically active; @c false otherwise.
		*/
		void set_ready_line(bool active);

	private:
		T &bus_handler_;
};

#include "Implementation/6502Implementation.hpp"

}
}

#endif /* MOS6502_cpp */
