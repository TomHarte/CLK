//
//  6502.hpp
//  CLK
//
//  Created by Thomas Harte on 09/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>

#include "../6502Esque/6502Esque.hpp"
#include "../6502Esque/Implementation/LazyFlags.hpp"
#include "../../Numeric/Carry.hpp"
#include "../../Numeric/RegisterSizes.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

namespace CPU::MOS6502 {

// Adopt a bunch of things from MOS6502Esque.
using BusOperation = CPU::MOS6502Esque::BusOperation;
using BusHandler = CPU::MOS6502Esque::BusHandler<uint16_t>;
using Register = CPU::MOS6502Esque::Register;
using Flag = CPU::MOS6502Esque::Flag;

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

constexpr bool has_decimal_mode(Personality p)	{	return p >= Personality::P6502;				}
constexpr bool is_65c02(Personality p)			{	return p >= Personality::PSynertek65C02;	}
constexpr bool has_bbrbbsrmbsmb(Personality p)	{	return p >= Personality::PRockwell65C02;	}
constexpr bool has_stpwai(Personality p)		{	return p >= Personality::PWDC65C02;			}

/*!
	An opcode that is guaranteed to cause a 6502 to jam.
*/
constexpr uint8_t JamOpcode = 0xf2;

#include "Implementation/6502Storage.hpp"

/*!
	A base class from which the 6502 descends; separated for implementation reasons only.
*/
class ProcessorBase: public ProcessorStorage {
	public:
		ProcessorBase(Personality personality) : ProcessorStorage(personality) {}

		/*!
			Gets the value of a register.

			@see set_value_of

			@param r The register to set.
			@returns The value of the register. 8-bit registers will be returned as unsigned.
		*/
		inline uint16_t value_of(Register r) const;

		/*!
			Sets the value of a register.

			@see value_of

			@param r The register to set.
			@param value The value to set. If the register is only 8 bit, the value will be truncated.
		*/
		inline void set_value_of(Register r, uint16_t value);

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
		inline bool is_jammed() const;

		/*!
			FOR TESTING PURPOSES ONLY: forces the processor into a state where
			the next thing it intends to do is fetch a new opcode.
		*/
		inline void restart_operation_fetch();
};

/*!
	@abstact Template providing emulation of a 6502 processor.

	@discussion Users should provide as the first template parameter a subclass of CPU::MOS6502::BusHandler; the 6502
	will announce its cycle-by-cycle activity via the bus handler, which is responsible for marrying it to a bus. They
	can also nominate whether the processor includes support for the ready line. Declining to support the ready line
	can produce a minor runtime performance improvement.
*/
template <Personality personality, typename BusHandler, bool uses_ready_line> class Processor: public ProcessorBase {
	public:
		/*!
			Constructs an instance of the 6502 that will use @c bus_handler for all bus communications.
		*/
		Processor(BusHandler &bus_handler) : ProcessorBase(personality), bus_handler_(bus_handler) {}

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
		BusHandler &bus_handler_;
};

#include "Implementation/6502Implementation.hpp"

}
