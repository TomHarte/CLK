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
		/// The final half cycle of the opcode fetch part of an M1 cycle.
		ReadOpcode = 0,
		/// The 1.5 cycles of a read cycle.
		Read,
		/// The 1.5 cycles of a write cycle.
		Write,
		/// The 1.5 cycles of an input cycle.
		Input,
		/// The 1.5 cycles of an output cycle.
		Output,
		/// The 1.5 cycles of an interrupt acknowledgment.
		Interrupt,

		/// The two-cycle refresh part of an M1 cycle.
		Refresh,
		/// A period with no changes in bus signalling.
		Internal,
		/// A bus acknowledgement cycle.
		BusAcknowledge,

		/// A wait state within an M1 cycle.
		ReadOpcodeWait,
		/// A wait state within a read cycle.
		ReadWait,
		/// A wait state within a write cycle.
		WriteWait,
		/// A wait state within an input cycle.
		InputWait,
		/// A wait state within an output cycle.
		OutputWait,
		/// A wait state within an interrupt acknowledge cycle.
		InterruptWait,

		/// The first 1.5 cycles of an M1 bus cycle, up to the sampling of WAIT.
		ReadOpcodeStart,
		/// The first 1.5 cycles of a read cycle, up to the sampling of WAIT.
		ReadStart,
		/// The first 1.5 cycles of a write cycle, up to the sampling of WAIT.
		WriteStart,
		/// The first 1.5 samples of an input bus cycle, up to the sampling of WAIT.
		InputStart,
		/// The first 1.5 samples of an output bus cycle, up to the sampling of WAIT.
		OutputStart,
		/// The first portion of an interrupt acknowledgement — 2.5 or 3.5 cycles, depending on interrupt mode.
		InterruptStart,
	};
	/// The operation being carried out by the Z80. See the various getters below for better classification.
	const Operation operation = Operation::Internal;
	/// The length of this operation.
	const HalfCycles length;
	/// The current value of the address bus.
	const uint16_t *const address = nullptr;
	/// If the Z80 is outputting to the data bus, a pointer to that value. Otherwise, a pointer to the location where the current data bus value should be placed.
	uint8_t *const value = nullptr;
	/// @c true if this operation is occurring only because of an external request; @c false otherwise.
	const bool was_requested = false;

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

	enum Line {
		CLK = 1 << 0,

		MREQ = 1 << 1,
		IOREQ = 1 << 2,

		RD = 1 << 3,
		WR = 1 << 4,
		RFSH = 1 << 5,

		M1 = 1 << 6,

		BUSACK = 1 << 7,
	};

	/// @returns A C-style array of the bus state at the beginning of each half cycle in this
	/// partial machine cycle. Each element is a combination of bit masks from the Line enum;
	/// bit set means line active, bit clear means line inactive. For the CLK line set means high.
	const uint8_t *bus_state() const {
		switch(operation) {

			//
			// M1 cycle
			//

			case Operation::ReadOpcodeStart: {
				static constexpr uint8_t states[] = {
					Line::CLK |	Line::M1,
								Line::M1 |	Line::MREQ |	Line::RD,
					Line::CLK |	Line::M1 |	Line::MREQ |	Line::RD,
				};
				return states;
			}

			case Operation::ReadOpcode:
			case Operation::ReadOpcodeWait: {
				static constexpr uint8_t states[] = {
								Line::M1 |	Line::MREQ |	Line::RD,
					Line::CLK |	Line::M1 |	Line::MREQ |	Line::RD,
				};
				return states;
			}

			case Operation::Refresh: {
				static constexpr uint8_t states[] = {
					Line::CLK |	Line::RFSH,
								Line::RFSH |	Line::MREQ,
					Line::CLK |	Line::RFSH |	Line::MREQ,
								Line::RFSH,
					Line::CLK |	Line::RFSH,
								Line::RFSH,
				};
				return states;
			}

			//
			// Read cycle.
			//

			case Operation::ReadStart: {
				static constexpr uint8_t states[] = {
					Line::CLK,
								Line::RD |	Line::MREQ,
					Line::CLK |	Line::RD |	Line::MREQ,
				};
				return states;
			}

			case Operation::ReadWait: {
				static constexpr uint8_t states[] = {
								Line::MREQ |	Line::RD,
					Line::CLK |	Line::MREQ |	Line::RD,
								Line::MREQ |	Line::RD,
					Line::CLK |	Line::MREQ |	Line::RD,
								Line::MREQ |	Line::RD,
					Line::CLK |	Line::MREQ |	Line::RD,
				};
				return states;
			}

			case Operation::Read: {
				static constexpr uint8_t states[] = {
								Line::MREQ |	Line::RD,
					Line::CLK |	Line::MREQ |	Line::RD,
								0,
				};
				return states;
			}

			//
			// Write cycle.
			//

			case Operation::WriteStart: {
				static constexpr uint8_t states[] = {
					Line::CLK,
								Line::MREQ,
					Line::CLK |	Line::MREQ,
				};
				return states;
			}

			case Operation::WriteWait: {
				static constexpr uint8_t states[] = {
								Line::MREQ,
					Line::CLK |	Line::MREQ,
								Line::MREQ,
					Line::CLK |	Line::MREQ,
								Line::MREQ,
					Line::CLK |	Line::MREQ,
				};
				return states;
			}

			case Operation::Write: {
				static constexpr uint8_t states[] = {
								Line::MREQ |	Line::WR,
					Line::CLK |	Line::MREQ |	Line::WR,
								0,
				};
				return states;
			}

			//
			// Input cycle.
			//

			case Operation::InputStart: {
				static constexpr uint8_t states[] = {
					Line::CLK,
								0,
					Line::CLK |	Line::IOREQ |	Line::RD,
				};
				return states;
			}

			case Operation::InputWait: {
				static constexpr uint8_t states[] = {
								Line::IOREQ |	Line::RD,
					Line::CLK |	Line::IOREQ |	Line::RD,
				};
				return states;
			}

			case Operation::Input: {
				static constexpr uint8_t states[] = {
								Line::IOREQ |	Line::RD,
					Line::CLK |	Line::IOREQ |	Line::RD,
								0,
				};
				return states;
			}

			//
			// Output cycle.
			//

			case Operation::OutputStart: {
				static constexpr uint8_t states[] = {
					Line::CLK,
								0,
					Line::CLK |	Line::IOREQ |	Line::WR,
				};
				return states;
			}

			case Operation::OutputWait: {
				static constexpr uint8_t states[] = {
								Line::IOREQ |	Line::WR,
					Line::CLK |	Line::IOREQ |	Line::WR,
				};
				return states;
			}

			case Operation::Output: {
				static constexpr uint8_t states[] = {
								Line::IOREQ |	Line::WR,
					Line::CLK |	Line::IOREQ |	Line::WR,
								0,
				};
				return states;
			}

			//
			// TODO: Interrupt acknowledge.
			//

			//
			// Bus acknowldge.
			//

			case Operation::BusAcknowledge: {
				static constexpr uint8_t states[] = {
					Line::CLK |	Line::BUSACK,
								Line::BUSACK,
				};
				return states;
			}

			//
			// Internal.
			//

			case Operation::Internal: {
				static constexpr uint8_t states[] = {
					Line::CLK, 0,
					Line::CLK, 0,
					Line::CLK, 0,
					Line::CLK, 0,
					Line::CLK, 0,
				};
				return states;
			}

			default: break;
		}

		static constexpr uint8_t none[] = {};
		return none;
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
		HalfCycles perform_machine_cycle([[maybe_unused]] const PartialMachineCycle &cycle) {
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
