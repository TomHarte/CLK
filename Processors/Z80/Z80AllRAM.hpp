//
//  Z80AllRAM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Z80AllRAM_hpp
#define Z80AllRAM_hpp

#include "Z80.hpp"
#include "../AllRAMProcessor.hpp"

namespace CPU {
namespace Z80 {

class AllRAMProcessor:
	public ::CPU::AllRAMProcessor {

	public:
		static AllRAMProcessor *Processor();

		struct MemoryAccessDelegate {
			virtual void z80_all_ram_processor_did_perform_bus_operation(AllRAMProcessor &processor, MachineCycle::Operation operation, uint16_t address, uint8_t value, int time_stamp) = 0;
		};
		inline void set_memory_access_delegate(MemoryAccessDelegate *delegate) {
			delegate_ = delegate;
		}

		virtual void run_for_cycles(int cycles) = 0;
		virtual uint16_t get_value_of_register(Register r) = 0;
		virtual void set_value_of_register(Register r, uint16_t value) = 0;
		virtual bool get_halt_line() = 0;
		virtual void reset_power_on() = 0;
		virtual void set_interrupt_line(bool value) = 0;
		virtual void set_non_maskable_interrupt_line(bool value) = 0;

		virtual int get_length_of_completed_machine_cycles() = 0;

	protected:
		MemoryAccessDelegate *delegate_;
		AllRAMProcessor() : ::CPU::AllRAMProcessor(65536), delegate_(nullptr) {}
};

}
}

#endif /* Z80AllRAM_hpp */
