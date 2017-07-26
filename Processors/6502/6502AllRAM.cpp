//
//  6502AllRAM.cpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#include "6502AllRAM.hpp"
#include <algorithm>
#include <string.h>

using namespace CPU::MOS6502;

namespace {

class ConcreteAllRAMProcessor: public AllRAMProcessor, public Processor<ConcreteAllRAMProcessor> {
	public:
		ConcreteAllRAMProcessor() {
			set_power_on(false);
		}

		inline Cycles perform_bus_operation(BusOperation operation, uint16_t address, uint8_t *value) {
			timestamp_++;

			if(operation == BusOperation::ReadOpcode) {
				check_address_for_trap(address);
			}

			if(isReadOperation(operation)) {
				*value = memory_[address];
			} else {
				memory_[address] = *value;
			}

			return Cycles(1);
		}

		void run_for(const Cycles &cycles) {
			Processor<ConcreteAllRAMProcessor>::run_for(cycles);
		}

		bool is_jammed() {
			return Processor<ConcreteAllRAMProcessor>::is_jammed();
		}

		void set_irq_line(bool value) {
			Processor<ConcreteAllRAMProcessor>::set_irq_line(value);
		}

		void set_nmi_line(bool value) {
			Processor<ConcreteAllRAMProcessor>::set_nmi_line(value);
		}

		void return_from_subroutine() {
			Processor<ConcreteAllRAMProcessor>::return_from_subroutine();
		}

		uint16_t get_value_of_register(Register r) {
			return Processor<ConcreteAllRAMProcessor>::get_value_of_register(r);
		}

		void set_value_of_register(Register r, uint16_t value) {
			Processor<ConcreteAllRAMProcessor>::set_value_of_register(r, value);
		}
};

}

AllRAMProcessor *AllRAMProcessor::Processor() {
	return new ConcreteAllRAMProcessor;
}
