//
//  6502Selector.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "6502Esque.hpp"
#include "../6502/6502.hpp"
#include "../65816/65816.hpp"

namespace CPU::MOS6502Esque {

enum class Type {
	TNES6502,			// the NES's 6502, which is like a 6502 but lacks decimal mode (though it retains the decimal flag)
	T6502,				// the original [NMOS] 6502, replete with various undocumented instructions
	TSynertek65C02,		// a 6502 extended with BRA, P[H/L][X/Y], STZ, TRB, TSB and the (zp) addressing mode and a few other additions
	TRockwell65C02,		// like the Synertek, but with BBR, BBS, RMB and SMB
	TWDC65C02,			// like the Rockwell, but with STP and WAI
	TWDC65816,			// the slightly 16-bit follow-up to the 6502
};

/*
	Machines that can use either a 6502 or a 65816 can use CPU::MOS6502Esque::Processor in order to select the proper
	class in much the same way that a raw user of CPU::MOS6502::Processor would set the personality. Just provide one
	of the type enums as above as the appropriate template parameter.
*/

template <Type processor_type, typename BusHandler, bool uses_ready_line> class Processor:
	public CPU::MOS6502::Processor<CPU::MOS6502::Personality(processor_type), BusHandler, uses_ready_line> {
		using CPU::MOS6502::Processor<CPU::MOS6502::Personality(processor_type), BusHandler, uses_ready_line>::Processor;
};

template <typename BusHandler, bool uses_ready_line> class Processor<Type::TWDC65816, BusHandler, uses_ready_line>:
	public CPU::WDC65816::Processor<BusHandler, uses_ready_line> {
		using CPU::WDC65816::Processor<BusHandler, uses_ready_line>::Processor;
};

/*
	Using BusHandlerT allows bus size to be defaulted by processor type.
*/
template <Type processor_type> class BusHandlerT: public BusHandler<uint16_t> {};
template <> class BusHandlerT<Type::TWDC65816>: public BusHandler<uint32_t> {};

/*
	Query for implemented registers.
*/
constexpr bool has(Type processor_type, Register r) {
	switch(r) {
		case Register::LastOperationAddress:
		case Register::ProgramCounter:
		case Register::StackPointer:
		case Register::Flags:
		case Register::A:
		case Register::X:
		case Register::Y:
			return true;

		case Register::EmulationFlag:
		case Register::DataBank:
		case Register::ProgramBank:
		case Register::Direct:
			return processor_type == Type::TWDC65816;
	}
}

constexpr bool has_extended_bus_output(Type processor_type) {
	return processor_type == Type::TWDC65816;
}

}
