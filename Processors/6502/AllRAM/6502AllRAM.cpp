//
//  6502AllRAM.cpp
//  CLK
//
//  Created by Thomas Harte on 13/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#include "6502AllRAM.hpp"

#include <algorithm>
#include <cstring>

using namespace CPU::MOS6502;

namespace {

using Type = CPU::MOS6502Esque::Type;

template <Type type> class ConcreteAllRAMProcessor: public AllRAMProcessor, public BusHandler {
	public:
		ConcreteAllRAMProcessor() :
			mos6502_(*this) {
			mos6502_.set_power_on(false);
		}

		inline Cycles perform_bus_operation(BusOperation operation, uint16_t address, uint8_t *value) {
			timestamp_ += Cycles(1);

			if(operation == BusOperation::ReadOpcode) {
				// TEMPORARY LOGGING. TODO: remove.
				printf("[%04x] %02x a:%04x x:%04x y:%04x p:%02x\n", address, memory_[address],
					mos6502_.get_value_of_register(Register::A),
					mos6502_.get_value_of_register(Register::X),
					mos6502_.get_value_of_register(Register::Y),
					mos6502_.get_value_of_register(Register::Flags));
				check_address_for_trap(address);
			}

			if(isReadOperation(operation)) {
				*value = memory_[address];
			} else {
				memory_[address] = *value;
			}

			return Cycles(1);
		}

		void run_for(const Cycles cycles) {
			mos6502_.run_for(cycles);
		}

		bool is_jammed() {
			return mos6502_.is_jammed();
		}

		void set_irq_line(bool value) {
			mos6502_.set_irq_line(value);
		}

		void set_nmi_line(bool value) {
			mos6502_.set_nmi_line(value);
		}

		uint16_t get_value_of_register(Register r) {
			return mos6502_.get_value_of_register(r);
		}

		void set_value_of_register(Register r, uint16_t value) {
			mos6502_.set_value_of_register(r, value);
		}

	private:
		CPU::MOS6502Esque::Processor<type, ConcreteAllRAMProcessor, false> mos6502_;
};

}

AllRAMProcessor *AllRAMProcessor::Processor(Type type) {
#define Bind(p) case p: return new ConcreteAllRAMProcessor<p>();
	switch(type) {
		default:
		Bind(Type::T6502)
		Bind(Type::TNES6502)
		Bind(Type::TSynertek65C02)
		Bind(Type::TWDC65C02)
		Bind(Type::TRockwell65C02)
		Bind(Type::TWDC65816)
	}
#undef Bind
}
