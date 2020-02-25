//
//  Z80AllRAM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Z80AllRAM.hpp"
#include <algorithm>

using namespace CPU::Z80;
namespace {

class ConcreteAllRAMProcessor: public AllRAMProcessor, public BusHandler {
	public:
		ConcreteAllRAMProcessor() : AllRAMProcessor(), z80_(*this) {}

		inline HalfCycles perform_machine_cycle(const PartialMachineCycle &cycle) {
			timestamp_ += cycle.length;
			if(!cycle.is_terminal()) {
				return HalfCycles(0);
			}

			uint16_t address = cycle.address ? *cycle.address : 0x0000;
			switch(cycle.operation) {
				case PartialMachineCycle::ReadOpcode:
					check_address_for_trap(address);
				case PartialMachineCycle::Read:
					*cycle.value = memory_[address];
				break;
				case PartialMachineCycle::Write:
					memory_[address] = *cycle.value;
				break;

				case PartialMachineCycle::Output:
				break;
				case PartialMachineCycle::Input:
					*cycle.value = port_delegate_ ? port_delegate_->z80_all_ram_processor_input(address) : 0xff;
				break;

				case PartialMachineCycle::Internal:
				case PartialMachineCycle::Refresh:
				break;

				case PartialMachineCycle::Interrupt:
					// A pick that means LD HL, (nn) if interpreted as an instruction but is otherwise
					// arbitrary.
					*cycle.value = 0x21;
				break;

				default:
				break;
			}

			if(memory_delegate_ != nullptr) {
				memory_delegate_->z80_all_ram_processor_did_perform_bus_operation(*this, cycle.operation, address, cycle.value ? *cycle.value : 0x00, timestamp_);
			}

			return HalfCycles(0);
		}

		void run_for(const Cycles cycles) final {
			z80_.run_for(cycles);
		}

		void run_for_instruction() final {
			int toggles = 0;
			int cycles = 0;

			// Run:
			//	(1) until is_starting_new_instruction is true;
			//	(2) until it is false again; and
			//	(3) until it is true again.
			while(true) {
				if(z80_.is_starting_new_instruction() != (toggles&1)) {
					++toggles;
					if(toggles == 3) break;
				}
				z80_.run_for(Cycles(1));
				++cycles;
			}
		}

		uint16_t get_value_of_register(Register r) final {
			return z80_.get_value_of_register(r);
		}

		void set_value_of_register(Register r, uint16_t value) final {
			z80_.set_value_of_register(r, value);
		}

		bool get_halt_line() final {
			return z80_.get_halt_line();
		}

		void reset_power_on() final {
			return z80_.reset_power_on();
		}

		void set_interrupt_line(bool value) final {
			z80_.set_interrupt_line(value);
		}

		void set_non_maskable_interrupt_line(bool value) final {
			z80_.set_non_maskable_interrupt_line(value);
		}

		void set_wait_line(bool value) final {
			z80_.set_wait_line(value);
		}

	private:
		CPU::Z80::Processor<ConcreteAllRAMProcessor, false, true> z80_;
		bool was_m1_ = false;
};

}

AllRAMProcessor *AllRAMProcessor::Processor() {
	return new ConcreteAllRAMProcessor;
}
