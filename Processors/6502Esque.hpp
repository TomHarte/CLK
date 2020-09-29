//
//  6502Esque.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef m6502Esque_h
#define m6502Esque_h

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
	Y
};

/*!
	Bus handlers will be given the task of performing bus operations, allowing them to provide whatever interface they like
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
#define isReadOperation(v)	(v == CPU::MOS6502Esque::BusOperation::Read || v == CPU::MOS6502Esque::BusOperation::ReadOpcode)

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
