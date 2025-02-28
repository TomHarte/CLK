//
//  6502AllRAM.hpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/6502Esque/6502Selector.hpp"
#include "Processors/AllRAMProcessor.hpp"

namespace CPU::MOS6502 {

class AllRAMProcessor:
	public ::CPU::AllRAMProcessor {
	public:
		static AllRAMProcessor *Processor(CPU::MOS6502Esque::Type type, bool has_cias = false);
		virtual ~AllRAMProcessor() = default;

		virtual void run_for(const Cycles cycles) = 0;
		virtual void run_for_instructions(int) = 0;
		virtual bool is_jammed() = 0;
		virtual void set_irq_line(bool value) = 0;
		virtual void set_nmi_line(bool value) = 0;
		virtual uint16_t value_of(Register r) = 0;
		virtual void set_value_of(Register r, uint16_t value) = 0;

	protected:
		AllRAMProcessor(size_t memory_size) : ::CPU::AllRAMProcessor(memory_size) {}
};

}
