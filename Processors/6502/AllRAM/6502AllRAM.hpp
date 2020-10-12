//
//  6502AllRAM.hpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#ifndef MOS6502AllRAM_cpp
#define MOS6502AllRAM_cpp

#include "../../6502Esque/6502Selector.hpp"
#include "../../AllRAMProcessor.hpp"

namespace CPU {
namespace MOS6502 {

class AllRAMProcessor:
	public ::CPU::AllRAMProcessor {
	public:
		static AllRAMProcessor *Processor(CPU::MOS6502Esque::Type type);
		virtual ~AllRAMProcessor() {}

		virtual void run_for(const Cycles cycles) = 0;
		virtual bool is_jammed() = 0;
		virtual void set_irq_line(bool value) = 0;
		virtual void set_nmi_line(bool value) = 0;
		virtual uint16_t get_value_of_register(Register r) = 0;
		virtual void set_value_of_register(Register r, uint16_t value) = 0;

	protected:
		AllRAMProcessor(size_t memory_size) : ::CPU::AllRAMProcessor(memory_size) {}
};

}
}

#endif /* MOS6502AllRAM_cpp */
