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
		RAM68000() : m68000_(*this) {
			// Setup the /RESET vector.
			ram_[0] = 0;
			ram_[1] = 0x206;	// Supervisor stack pointer.
			ram_[2] = 0;
			ram_[3] = 0x1000;	// Initial PC.

			// Ensure the condition codes start unset.
			auto state = get_processor_state();
			state.registers.status &= ~ConditionCode::AllConditions;
			set_processor_state(state);
		}

		uint32_t initial_pc() const {
			return 0x1000;
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

		void will_perform(uint32_t, uint16_t) {
			if(!has_run_) {
				m68000_.set_state(initial_state_);
			}

			--instructions_remaining_;
			if(!instructions_remaining_) {
				captured_state_ = m68000_.get_state();
			}
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
				m68000_.run_for(HalfCycles(80));
				duration_ -= HalfCycles(80);
				has_run_ = true;
			}
		}

		uint16_t *ram_at(uint32_t address) {
			return &ram_[(address >> 1) % ram_.size()];
		}

		HalfCycles perform_bus_operation(const CPU::MC68000Mk2::Microcycle &cycle, int) {
			const uint32_t word_address = cycle.word_address();
			if(instructions_remaining_) duration_ += cycle.length;

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
			return captured_state_;
		}

		void set_processor_state(const CPU::MC68000Mk2::State &state) {
			initial_state_ = captured_state_ = state;
			m68000_.set_state(state);
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
		CPU::MC68000Mk2::Processor<RAM68000, true, true, true> m68000_;
		std::array<uint16_t, 256*1024> ram_{};
		int instructions_remaining_;
		HalfCycles duration_;
		bool has_run_ = false;
		CPU::MC68000Mk2::State captured_state_, initial_state_;
};

#endif /* TestRunner68000_h */
