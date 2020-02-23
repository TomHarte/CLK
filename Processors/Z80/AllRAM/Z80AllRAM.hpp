//
//  Z80AllRAM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Z80AllRAM_hpp
#define Z80AllRAM_hpp

#include "../Z80.hpp"
#include "../../AllRAMProcessor.hpp"

namespace CPU {
namespace Z80 {

class AllRAMProcessor:
	public ::CPU::AllRAMProcessor {

	public:
		static AllRAMProcessor *Processor();

		struct MemoryAccessDelegate {
			virtual void z80_all_ram_processor_did_perform_bus_operation(CPU::Z80::AllRAMProcessor &processor, CPU::Z80::PartialMachineCycle::Operation operation, uint16_t address, uint8_t value, HalfCycles time_stamp) = 0;
		};
		inline void set_memory_access_delegate(MemoryAccessDelegate *delegate) {
			memory_delegate_ = delegate;
		}

		struct PortAccessDelegate {
			virtual uint8_t z80_all_ram_processor_input(uint16_t port) { return 0xff; }
		};
		inline void set_port_access_delegate(PortAccessDelegate *delegate) {
			port_delegate_ = delegate;
		}

		virtual void run_for(const Cycles cycles) = 0;
		virtual uint16_t get_value_of_register(Register r) = 0;
		virtual void set_value_of_register(Register r, uint16_t value) = 0;
		virtual bool get_halt_line() = 0;
		virtual void reset_power_on() = 0;

		virtual void set_interrupt_line(bool value) = 0;
		virtual void set_non_maskable_interrupt_line(bool value) = 0;
		virtual void set_wait_line(bool value) = 0;

	protected:
		MemoryAccessDelegate *memory_delegate_ = nullptr;
		PortAccessDelegate *port_delegate_ = nullptr;
		AllRAMProcessor() : ::CPU::AllRAMProcessor(65536) {}
};

}
}

#endif /* Z80AllRAM_hpp */
