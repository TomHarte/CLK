//
//  Z80.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Z80_hpp
#define Z80_hpp

#include <cassert>
#include <vector>
#include <cstdint>

#include "../RegisterSizes.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../../ClockReceiver/ForceInline.hpp"

namespace CPU {
namespace Z80 {

/*
	The list of registers that can be accessed via @c set_value_of_register and @c set_value_of_register.
*/
enum class Register {
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

	IFF1,	IFF2,	IM,

	MemPtr
};

/*
	Flags as defined on the Z80; can be used to decode the result of getting or setting @c Flags.
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
	Subclasses will be given the task of performing partial machine cycles, allowing them to provide whatever interface they like
	between a Z80 and the rest of the system. @c PartialMachineCycle defines the information they will be handed for each unit
	of execution.
*/
struct PartialMachineCycle {
	enum Operation {
		ReadOpcode = 0,
		Read,
		Write,
		Input,
		Output,
		Interrupt,

		Refresh,
		Internal,
		BusAcknowledge,

		ReadOpcodeWait,
		ReadWait,
		WriteWait,
		InputWait,
		OutputWait,
		InterruptWait,

		ReadOpcodeStart,
		ReadStart,
		WriteStart,
		InputStart,
		OutputStart,
		InterruptStart,
	};
	/// The operation being carried out by the Z80. See the various getters below for better classification.
	const Operation operation;
	/// The length of this operation.
	const HalfCycles length;
	/// The current value of the address bus.
	const uint16_t *const address;
	/// If the Z80 is outputting to the data bus, a pointer to that value. Otherwise, a pointer to the location where the current data bus value should be placed.
	uint8_t *const value;
	/// @c true if this operation is occurring only because of an external request; @c false otherwise.
	const bool was_requested;

	/*!
		@returns @c true if the processor believes that the bus handler should actually do something with
		the content of this PartialMachineCycle; @c false otherwise.
	*/
	forceinline bool expects_action() const {
		return operation <= Operation::Interrupt;
	}
	/*!
		@returns @c true if this partial machine cycle completes one of the documented full machine cycles;
		@c false otherwise.
	*/
	forceinline bool is_terminal() const {
		return operation <= Operation::BusAcknowledge;
	}
	/*!
		@returns @c true if this partial machine cycle is a wait cycle; @c false otherwise.
	*/
	forceinline bool is_wait() const {
		return operation >= Operation::ReadOpcodeWait && operation <= Operation::InterruptWait;
	}

	PartialMachineCycle(const PartialMachineCycle &rhs) noexcept;
	PartialMachineCycle(Operation operation, HalfCycles length, uint16_t *address, uint8_t *value, bool was_requested) noexcept;
	PartialMachineCycle() noexcept;
};

/*!
	A class providing empty implementations of the methods a Z80 uses to access the bus. To wire the Z80 to a bus,
	machines should subclass BusHandler and then declare a realisation of the Z80 template, supplying their bus
	handler.
*/
class BusHandler {
	public:
		/*!
			Announces that the Z80 has performed the partial machine cycle defined by @c cycle.

			@returns The number of additional HalfCycles that passed in objective time while this Z80 operation was ongoing.
			On an archetypal machine this will be HalfCycles(0) but some architectures may choose not to clock the Z80
			during some periods or may impose wait states so predictably that it's more efficient just to add them
			via this mechanism.
		*/
		HalfCycles perform_machine_cycle(const PartialMachineCycle &cycle) {
			return HalfCycles(0);
		}

		/*!
			Announces completion of all the cycles supplied to a .run_for request on the Z80. Intended to allow
			bus handlers to perform any deferred output work.
		*/
		void flush() {}
};

#include "Implementation/Z80Storage.hpp"

/*!
	A base class from which the Z80 descends; separated for implementation reasons only.
*/
class ProcessorBase: public ProcessorStorage {
	public:
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
			Gets the value of the HALT output line.
		*/
		inline bool get_halt_line() const;

		/*!
			Sets the logical value of the interrupt line.

			@param offset If called while within perform_machine_cycle this may be a value indicating
			how many cycles before now the line changed state. The value may not be longer than the
			current machine cycle. If called at any other time, this must be zero.
		*/
		inline void set_interrupt_line(bool value, HalfCycles offset = 0);

		/*!
			Gets the value of the interrupt line.
		*/
		inline bool get_interrupt_line() const;

		/*!
			Sets the logical value of the non-maskable interrupt line.

			@param offset See discussion in set_interrupt_line.
		*/
		inline void set_non_maskable_interrupt_line(bool value, HalfCycles offset = 0);

		/*!
			Gets the value of the non-maskable interrupt line.
		*/
		inline bool get_non_maskable_interrupt_line() const;

		/*!
			Sets the logical value of the reset line.
		*/
		inline void set_reset_line(bool value);

		/*!
			Gets whether the Z80 would reset at the next opportunity.

			@returns @c true if the line is logically active; @c false otherwise.
		*/
		bool get_is_resetting() const;

		/*!
			This emulation automatically sets itself up in power-on state at creation, which has the effect of triggering a
			reset at the first opportunity. Use @c reset_power_on to disable that behaviour.
		*/
		void reset_power_on();

		/*!
			@returns @c true if the Z80 is currently beginning to fetch a new instruction; @c false otherwise.

			This is not a speedy operation.
		*/
		bool is_starting_new_instruction() const;
};

/*!
	@abstact Template providing emulation of a Z80 processor.

	@discussion Users should provide as the first template parameter a subclass of CPU::Z80::BusHandler; the Z80
	will announce its activity via the bus handler, which is responsible for marrying it to a bus. Users
	can also nominate whether the processor includes support for the bus request and/or wait lines. Declining to
	support either can produce a minor runtime performance improvement.
*/
template <class T, bool uses_bus_request, bool uses_wait_line> class Processor: public ProcessorBase {
	public:
		Processor(T &bus_handler);

		/*!
			Runs the Z80 for a supplied number of cycles.

			@discussion Subclasses must implement @c perform_machine_cycle(const PartialMachineCycle &cycle) .

			If it is a read operation then @c value will be seeded with the value 0xff.

			@param cycles The number of cycles to run for.
		*/
		void run_for(const HalfCycles cycles);

		/*!
			Sets the logical value of the bus request line, having asserted that this Z80 supports the bus request line.
		*/
		void set_bus_request_line(bool value);

		/*!
			Gets the logical value of the bus request line.
		*/
		bool get_bus_request_line() const;

		/*!
			Sets the logical value of the wait line, having asserted that this Z80 supports the wait line.
		*/
		void set_wait_line(bool value);

		/*!
			Gets the logical value of the bus request line.
		*/
		bool get_wait_line() const;

	private:
		T &bus_handler_;

		void assemble_page(InstructionPage &target, InstructionTable &table, bool add_offsets);
		void copy_program(const MicroOp *source, std::vector<MicroOp> &destination);
};

#include "Implementation/Z80Implementation.hpp"

}
}

#endif /* Z80_hpp */
