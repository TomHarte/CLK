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

#include "../../../Processors/68000/68000.hpp"

using Flag = CPU::MC68000::Flag;

/*!
	Provides a 68000 with 64kb of RAM in its low address space;
	/RESET will put the supervisor stack pointer at 0xFFFF and
	begin execution at 0x0400.
*/
class RAM68000: public CPU::MC68000::BusHandler {
	public:
		RAM68000() : m68000_(*this) {
			// Setup the /RESET vector.
			ram_[0] = 0;
			ram_[1] = 0x206;	// Supervisor stack pointer.
			ram_[2] = 0;
			ram_[3] = 0x1000;	// Initial PC.

			// Ensure the condition codes start unset.
			auto state = get_processor_state();
			state.status &= ~Flag::ConditionCodes;
			set_processor_state(state);
		}

		void set_program(const std::vector<uint16_t> &program) {
			memcpy(&ram_[0x1000 >> 1], program.data(), program.size() * sizeof(uint16_t));

			// Add a NOP suffix, to avoid corrupting flags should the attempt to
			// run for a certain number of instructions overrun.
			ram_[(0x1000 >> 1) + program.size()] = 0x4e71;
		}

		void set_initial_stack_pointer(uint32_t sp) {
			ram_[0] = sp >> 16;
			ram_[1] = sp & 0xffff;
		}

		void will_perform(uint32_t address, uint16_t opcode) {
			--instructions_remaining_;
		}

		void run_for_instructions(int count) {
			instructions_remaining_ = count + (has_run_ ? 0 : 1);
			finish_reset_if_needed();

			while(instructions_remaining_) {
				run_for(HalfCycles(2));
			}
		}

		void run_for(HalfCycles cycles) {
			finish_reset_if_needed();
			m68000_.run_for(cycles);
		}

		void finish_reset_if_needed() {
			// If the 68000 hasn't run yet, build in the necessary
			// cycles to finish the reset program, and set the stored state.
			if(!has_run_) {
				has_run_ = true;
				m68000_.run_for(HalfCycles(76));
				duration_ -= HalfCycles(76);
			}
		}

		uint16_t *ram_at(uint32_t address) {
			return &ram_[(address >> 1) % ram_.size()];
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			const uint32_t word_address = cycle.word_address();
			if(instructions_remaining_) duration_ += cycle.length;

			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				if(cycle.operation & Microcycle::InterruptAcknowledge) {
					cycle.value->halves.low = 10;
				} else {
					switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
						default: break;

						case Microcycle::SelectWord | Microcycle::Read:
							cycle.value->full = ram_[word_address % ram_.size()];
						break;
						case Microcycle::SelectByte | Microcycle::Read:
							cycle.value->halves.low = ram_[word_address % ram_.size()] >> cycle.byte_shift();
						break;
						case Microcycle::SelectWord:
							ram_[word_address % ram_.size()] = cycle.value->full;
						break;
						case Microcycle::SelectByte:
							ram_[word_address % ram_.size()] = uint16_t(
								(cycle.value->halves.low << cycle.byte_shift()) |
								(ram_[word_address % ram_.size()] & cycle.untouched_byte_mask())
							);
						break;
					}
				}
			}

			return HalfCycles(0);
		}

		CPU::MC68000::Processor<RAM68000, true>::State get_processor_state() {
			return m68000_.get_state();
		}

		void set_processor_state(const CPU::MC68000::Processor<RAM68000, true>::State &state) {
			m68000_.set_state(state);
		}

		CPU::MC68000::Processor<RAM68000, true, true> &processor() {
			return m68000_;
		}

		int get_cycle_count() {
			return int(duration_.as_integral()) >> 1;
		}

	private:
		CPU::MC68000::Processor<RAM68000, true, true> m68000_;
		std::array<uint16_t, 256*1024> ram_{};
		int instructions_remaining_;
		HalfCycles duration_;
		bool has_run_ = false;
};

#endif /* TestRunner68000_h */
