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

#include "../../../Components/6526/6526.hpp"

//#define BE_NOISY
using namespace CPU::MOS6502;

namespace {

static constexpr bool LogAllReads = false;
static constexpr bool LogAllWrites = false;
static constexpr bool LogCIAAccesses = true;
static constexpr bool LogProgramCounter = false;

using Type = CPU::MOS6502Esque::Type;

template <Type type, bool has_cias> class ConcreteAllRAMProcessor: public AllRAMProcessor, public BusHandler {
	public:
		ConcreteAllRAMProcessor(size_t memory_size) :
			AllRAMProcessor(memory_size),
			mos6502_(*this),
			cia1_(cia1_handler_),
			cia2_(cia2_handler_) {
			mos6502_.set_power_on(false);
		}

		inline Cycles perform_bus_operation(BusOperation operation, uint32_t address, uint8_t *value) {
			timestamp_ += Cycles(1);

			if constexpr (has_cias) {
				cia1_.run_for(HalfCycles(2));
				cia2_.run_for(HalfCycles(2));
			}

			if(isAccessOperation(operation)) {
				if(operation == BusOperation::ReadOpcode) {
					if constexpr (LogProgramCounter) {
						printf("[%04x] %02x a:%04x x:%04x y:%04x p:%02x s:%02x\n", address, memory_[address],
							mos6502_.get_value_of_register(Register::A),
							mos6502_.get_value_of_register(Register::X),
							mos6502_.get_value_of_register(Register::Y),
							mos6502_.get_value_of_register(Register::Flags) & 0xff,
							mos6502_.get_value_of_register(Register::StackPointer) & 0xff);
					}
					check_address_for_trap(address);
					--instructions_;
				}

				if(isReadOperation(operation)) {
					*value = memory_[address];

					if constexpr (has_cias) {
						if((address & 0xff00) == 0xdc00) {
							*value = cia1_.read(address);
							if constexpr (LogCIAAccesses) {
								printf("[%d] CIA1: %04x -> %02x\n", timestamp_.as<int>(), address, *value);
							}
						} else if((address & 0xff00) == 0xdd00) {
							*value = cia2_.read(address);
							if constexpr (LogCIAAccesses) {
								printf("[%d] CIA2: %04x -> %02x\n", timestamp_.as<int>(), address, *value);
							}
						}
					}

					if constexpr (LogAllReads) {
//						if((address&0xff00) == 0x100) {
							printf("%04x -> %02x\n", address, *value);
//						}
					}
				} else {
					memory_[address] = *value;

					if constexpr (has_cias) {
						if((address & 0xff00) == 0xdc00) {
							cia1_.write(address, *value);
							if constexpr (LogCIAAccesses) {
								printf("[%d] CIA1: %04x <- %02x\n", timestamp_.as<int>(), address, *value);
							}
						} else if((address & 0xff00) == 0xdd00) {
							cia2_.write(address, *value);
							if constexpr (LogCIAAccesses) {
								printf("[%d] CIA2: %04x <- %02x\n", timestamp_.as<int>(), address, *value);
							}
						}
					}

					if constexpr (LogAllWrites) {
//						if((address&0xff00) == 0x100) {
							printf("%04x <- %02x\n", address, *value);
//						}
					}
				}
			}

			mos6502_.set_irq_line(cia1_.get_interrupt_line());
			mos6502_.set_nmi_line(cia2_.get_interrupt_line());

			return Cycles(1);
		}

		void run_for(const Cycles cycles) {
			mos6502_.run_for(cycles);
		}

		void run_for_instructions(int count) {
			instructions_ = count;
			while(instructions_) {
				mos6502_.run_for(Cycles(1));
			}
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
		int instructions_ = 0;

		class PortHandler: public MOS::MOS6526::PortHandler {};
		PortHandler cia1_handler_, cia2_handler_;

		MOS::MOS6526::MOS6526<PortHandler, MOS::MOS6526::Personality::P6526> cia1_, cia2_;
};

}

AllRAMProcessor *AllRAMProcessor::Processor(Type type, bool has_cias) {
#define Bind(p) \
	case p: \
		if(has_cias) return new ConcreteAllRAMProcessor<p, true>(type == Type::TWDC65816 ? 16*1024*1024 : 64*1024);	\
		else return new ConcreteAllRAMProcessor<p, false>(type == Type::TWDC65816 ? 16*1024*1024 : 64*1024);	\

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
