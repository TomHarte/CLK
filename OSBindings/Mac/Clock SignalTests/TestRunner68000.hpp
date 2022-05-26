//
//  TestRunner68000.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef TestRunner68000_h
#define TestRunner68000_h

#include <array>
#include <functional>
#include <vector>

#include "../../../Processors/68000Mk2/68000Mk2.hpp"

using namespace InstructionSet::M68k;

/*!
	Provides a 68000 with 64kb of RAM in its low address space;
	/RESET will put the supervisor stack pointer at 0xFFFF and
	begin execution at 0x0400.
*/
class RAM68000: public CPU::MC68000Mk2::BusHandler {
	public:
		RAM68000() : m68000_(*this) {}

		uint32_t initial_pc() const {
			return 0x1000;
		}

		void set_program(
			const std::vector<uint16_t> &program,
			uint32_t stack_pointer = 0x206
		) {
			memcpy(&ram_[0x1000 >> 1], program.data(), program.size() * sizeof(uint16_t));

			// Ensure the condition codes start unset and set the initial program counter
			// and supervisor stack pointer, as well as starting in supervisor mode.
			auto registers = m68000_.get_state().registers;
			registers.status &= ~ConditionCode::AllConditions;
			registers.status |= 0x2700;
			registers.program_counter = initial_pc();
			registers.supervisor_stack_pointer = stack_pointer;
			m68000_.decode_from_state(registers);
		}

		void set_registers(std::function<void(InstructionSet::M68k::RegisterSet &)> func) {
			auto state = m68000_.get_state();
			func(state.registers);
			m68000_.set_state(state);
		}

		void will_perform(uint32_t, uint16_t) {
			--instructions_remaining_;
			if(instructions_remaining_ < 0) {
				throw StopException();
			}
		}

		void run_for_instructions(int count) {
			duration_ = HalfCycles(0);
			instructions_remaining_ = count;
			if(!instructions_remaining_) return;

			try {
				while(true) {
					run_for(HalfCycles(2000));
				}
			} catch (const StopException &) {}
		}

		void run_for(HalfCycles cycles) {
			m68000_.run_for(cycles);
		}

		uint16_t *ram_at(uint32_t address) {
			return &ram_[(address >> 1) % ram_.size()];
		}

		HalfCycles perform_bus_operation(const CPU::MC68000Mk2::Microcycle &cycle, int) {
			const uint32_t word_address = cycle.word_address();
			duration_ += cycle.length;

			using Microcycle = CPU::MC68000Mk2::Microcycle;
			if(cycle.data_select_active()) {
				if(cycle.operation & Microcycle::InterruptAcknowledge) {
					cycle.value->b = 10;
				} else {
					switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
						default: break;

						case Microcycle::SelectWord | Microcycle::Read:
							cycle.value->w = ram_[word_address % ram_.size()];
						break;
						case Microcycle::SelectByte | Microcycle::Read:
							cycle.value->b = ram_[word_address % ram_.size()] >> cycle.byte_shift();
						break;
						case Microcycle::SelectWord:
							ram_[word_address % ram_.size()] = cycle.value->w;
						break;
						case Microcycle::SelectByte:
							ram_[word_address % ram_.size()] = uint16_t(
								(cycle.value->b << cycle.byte_shift()) |
								(ram_[word_address % ram_.size()] & cycle.untouched_byte_mask())
							);
						break;
					}
				}
			}

			return HalfCycles(0);
		}

		CPU::MC68000Mk2::State get_processor_state() {
			return m68000_.get_state();
		}

		auto &processor() {
			return m68000_;
		}

		int get_cycle_count() {
			return int(duration_.as_integral()) >> 1;
		}

		void reset_cycle_count() {
			duration_ = HalfCycles(0);
		}

	private:
		struct StopException {};

		CPU::MC68000Mk2::Processor<RAM68000, true, true, true> m68000_;
		std::array<uint16_t, 256*1024> ram_{};
		int instructions_remaining_;
		HalfCycles duration_;
};

#endif /* TestRunner68000_h */
