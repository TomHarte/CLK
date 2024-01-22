//
//  65816.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../../Numeric/RegisterSizes.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"
#include "../6502Esque/6502Esque.hpp"
#include "../6502Esque/Implementation/LazyFlags.hpp"

namespace CPU::WDC65816 {

using BusOperation = CPU::MOS6502Esque::BusOperation;
using Register = CPU::MOS6502Esque::Register;
using Flag = CPU::MOS6502Esque::Flag;

enum ExtendedBusOutput {
	Emulation = (1 << 0),
	MemorySize = (1 << 1),
	IndexSize = (1 << 2),
	MemoryLock = (1 << 3),
};

#include "Implementation/65816Storage.hpp"

class ProcessorBase: protected ProcessorStorage {
	public:
		inline void set_power_on(bool);
		inline void set_irq_line(bool);
		inline void set_nmi_line(bool);
		inline void set_reset_line(bool);
		inline void set_abort_line(bool);
		inline bool get_is_resetting() const;

		/*!
			Returns the current state of all lines not ordinarily pushed to the BusHandler,
			as listed in the ExtendedBusOutput enum.
		*/
		inline int get_extended_bus_output();

		/*!
			Provided for symmetry with the 6502; a 65816 is never jammed.
		*/
		inline bool is_jammed() const;

		/*!
			FOR TESTING PURPOSES ONLY: forces the processor into a state where
			the next thing it intends to do is fetch a new opcode.
		*/
		inline void restart_operation_fetch();

		void set_value_of(Register r, uint16_t value);
		uint16_t value_of(Register r) const;
};

template <typename BusHandler, bool uses_ready_line> class Processor: public ProcessorBase {
	public:
		/*!
			Constructs an instance of the 6502 that will use @c bus_handler for all bus communications.
		*/
		Processor(BusHandler &bus_handler) : bus_handler_(bus_handler) {}

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

#include "Implementation/65816Implementation.hpp"

}
