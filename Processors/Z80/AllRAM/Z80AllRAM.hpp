//
//  Z80AllRAM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/Z80/Z80.hpp"
#include "Processors/AllRAMProcessor.hpp"

namespace CPU::Z80 {

class AllRAMProcessor: public ::CPU::AllRAMProcessor {
public:
	static AllRAMProcessor *Processor();

	struct MemoryAccessDelegate {
		virtual void z80_all_ram_processor_did_perform_bus_operation(
			CPU::Z80::AllRAMProcessor &,
			CPU::Z80::PartialMachineCycle::Operation,
			uint16_t address,
			uint8_t value,
			HalfCycles time_stamp) = 0;
	};
	inline void set_memory_access_delegate(MemoryAccessDelegate *const delegate) {
		memory_delegate_ = delegate;
	}

	struct PortAccessDelegate {
		virtual uint8_t z80_all_ram_processor_input(uint16_t) { return 0xff; }
	};
	inline void set_port_access_delegate(PortAccessDelegate *delegate) {
		port_delegate_ = delegate;
	}

	virtual void run_for(const Cycles cycles) = 0;
	virtual void run_for_instruction() = 0;
	virtual uint16_t value_of(Register) = 0;
	virtual void set_value_of(Register, uint16_t) = 0;
	virtual bool get_halt_line() = 0;
	virtual void reset_power_on() = 0;

	virtual void set_interrupt_line(bool) = 0;
	virtual void set_non_maskable_interrupt_line(bool) = 0;
	virtual void set_wait_line(bool) = 0;

protected:
	MemoryAccessDelegate *memory_delegate_ = nullptr;
	PortAccessDelegate *port_delegate_ = nullptr;
	AllRAMProcessor() : ::CPU::AllRAMProcessor(65536) {}
};

}
