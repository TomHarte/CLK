//
//  6502AllRAM.hpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#ifndef MOS6502AllRAM_cpp
#define MOS6502AllRAM_cpp

#include "6502.hpp"
#include "../AllRAMProcessor.hpp"

namespace CPU {
namespace MOS6502 {

class AllRAMProcessor:
	public ::CPU::AllRAMProcessor {

	public:
		static AllRAMProcessor *Processor();

		virtual void run_for_cycles(int number_of_cycles) = 0;
		virtual bool is_jammed() = 0;
		virtual void set_irq_line(bool value) = 0;
		virtual void set_nmi_line(bool value) = 0;
		virtual void return_from_subroutine() = 0;
		virtual uint16_t get_value_of_register(Register r) = 0;
		virtual void set_value_of_register(Register r, uint16_t value) = 0;

	protected:
		AllRAMProcessor() : ::CPU::AllRAMProcessor(65536) {}
};

}
}

#endif /* MOS6502AllRAM_cpp */
