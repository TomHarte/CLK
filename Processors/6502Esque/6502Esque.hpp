//
//  6502Esque.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef m6502Esque_h
#define m6502Esque_h

#include "../../ClockReceiver/ClockReceiver.hpp"

/*
	This file defines how the CPU-controlled part of a bus looks for the 6502 and
	for other processors with a sufficiently-similar bus.

	I'm not yet a big fan of the name I've used here, and I'm still on the fence
	about what to do when eventually I get around to the 6800 and/or 6809, which have
	very similar bus characteristics.

	So: this is _very_ provisional stuff.
*/
namespace CPU {
namespace MOS6502Esque {

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
	Y,

	// These exist on a 65816 only.
	EmulationFlag,
	DataBank,
	ProgramBank,
	Direct
};

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
	Carry		= 0x01,

	// These are available on a 65816 only.
	MemorySize	= 0x20,
	IndexSize	= 0x10,
};

/*!
	Bus handlers will be given the task of performing bus operations, allowing them to provide whatever interface they like
	between a 6502-esque chip and the rest of the system. @c BusOperation lists the types of bus operation that may be requested.
*/
enum BusOperation {
	/// 6502: indicates that a read was signalled.
	/// 65816: indicates that a read was signalled with VDA.
	Read,
	/// 6502: indicates that a read was signalled with SYNC.
	/// 65816: indicates that a read was signalled with VDA and VPA.
	ReadOpcode,
	/// 6502: never signalled.
	/// 65816: indicates that a read was signalled with VPA.
	ReadProgram,
	/// 6502: never signalled.
	/// 65816: indicates that a read was signalled with VPB.
	ReadVector,
	/// 6502: never signalled.
	/// 65816: indicates that a read was signalled, but neither VDA nor VPA were active.
	InternalOperationRead,

	/// 6502: indicates that a write was signalled.
	/// 65816: indicates that a write was signalled with VDA.
	Write,
	/// 6502: never signalled.
	/// 65816: indicates that a write was signalled, but neither VDA nor VPA were active.
	InternalOperationWrite,

	/// All processors: indicates that the processor is paused due to the RDY input.
	/// 65C02 and 65816: indicates a WAI is ongoing.
	Ready,

	/// 65C02 and 65816: indicates a STP condition.
	None,
};

/*!
	For a machine watching only the RWB line, evaluates to @c true if the operation should be treated as a read; @c false otherwise.
*/
#define isReadOperation(v)		(v <= CPU::MOS6502Esque::InternalOperationRead)

/*!
	For a machine watching only the RWB line, evaluates to @c true if the operation is any sort of write; @c false otherwise.
*/
#define isWriteOperation(v)		(v == CPU::MOS6502Esque::Write || v == CPU::MOS6502Esque::InternalOperationWrite)

/*!
	Evaluates to @c true if the operation actually expects a response; @c false otherwise.
*/
#define isAccessOperation(v)	((v < CPU::MOS6502Esque::Ready) && (v != CPU::MOS6502Esque::InternalOperationRead) && (v != CPU::MOS6502Esque::InternalOperationWrite))

/*!
	A class providing empty implementations of the methods a 6502 uses to access the bus. To wire the 6502 to a bus,
	machines should subclass BusHandler and then declare a realisation of the 6502 template, suplying their bus
	handler.
*/
template <typename AddressType> class BusHandler {
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
		Cycles perform_bus_operation([[maybe_unused]] BusOperation operation, [[maybe_unused]] AddressType address, [[maybe_unused]] uint8_t *value) {
			return Cycles(1);
		}

		/*!
			Announces completion of all the cycles supplied to a .run_for request on the 6502. Intended to allow
			bus handlers to perform any deferred output work.
		*/
		void flush() {}
};

}
}

#endif /* m6502Esque_h */
