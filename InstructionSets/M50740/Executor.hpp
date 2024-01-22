//
//  Executor.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Instruction.hpp"
#include "Parser.hpp"
#include "../CachingExecutor.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace InstructionSet::M50740 {

class Executor;
using CachingExecutor = CachingExecutor<Executor, 0x1fff, 255, Instruction, false>;

struct PortHandler {
	virtual void run_ports_for(Cycles) = 0;
	virtual void set_port_output(int port, uint8_t value) = 0;
	virtual uint8_t get_port_input(int port) = 0;
};

/*!
	Executes M50740 code subject to heavy limitations:

		* the instruction stream cannot run across any of the specialised IO addresses; and
		* timing is correct to whole-opcode boundaries only.
*/
class Executor: public CachingExecutor {
	public:
		Executor(PortHandler &);
		void set_rom(const std::vector<uint8_t> &rom);

		void reset();
		void set_interrupt_line(bool);

		uint8_t get_output_mask(int port);

		/*!
			Runs, in discrete steps, the minimum number of instructions as it takes to complete at least @c cycles.
		*/
		void run_for(Cycles cycles);

	private:
		// MARK: - CachingExecutor-facing interface.

		friend CachingExecutor;

		/*!
			Maps instructions to performers; called by the CachingExecutor and for this instruction set, extremely trivial.
		*/
		inline PerformerIndex action_for(Instruction instruction) {
			// This is a super-simple processor, so the opcode can be used directly to index the performers.
			return instruction.opcode;
		}

		/*!
			Parses from @c start and no later than @c max_address, using the CachingExecutor as a target.
		*/
		inline void parse(uint16_t start, uint16_t closing_bound) {
			Parser<Executor, false> parser;
			parser.parse(*this, &memory_[0], start & 0x1fff, closing_bound);
		}

	private:
		// MARK: - Internal framework for generator performers.

		/*!
			Provides dynamic lookup of @c perform(Executor*).
		*/
		class PerformerLookup {
			public:
				PerformerLookup() {
					fill<int(MinOperation)>(performers_);
				}

				Performer performer(Operation operation, AddressingMode addressing_mode) {
					const auto index =
						(int(operation) - MinOperation) * (1 + MaxAddressingMode - MinAddressingMode) +
						(int(addressing_mode) - MinAddressingMode);
					return performers_[index];
				}

			private:
				Performer performers_[(1 + MaxAddressingMode - MinAddressingMode) * (1 + MaxOperation - MinOperation)];

				template<int operation, int addressing_mode> void fill_operation(Performer *target) {
					*target = &Executor::perform<Operation(operation), AddressingMode(addressing_mode)>;

					if constexpr (addressing_mode+1 <= MaxAddressingMode) {
						fill_operation<operation, addressing_mode+1>(target + 1);
					}
				}

				template<int operation> void fill(Performer *target) {
					fill_operation<operation, int(MinAddressingMode)>(target);
					target += 1 + MaxAddressingMode - MinAddressingMode;

					if constexpr (operation+1 <= MaxOperation) {
						fill<operation+1>(target);
					}
				}
		};
		inline static PerformerLookup performer_lookup_;

		/*!
			Performs @c operation using @c operand as the value fetched from memory, if any.
		*/
		template <Operation operation> void perform(uint8_t *operand);

		/*!
			Performs @c operation in @c addressing_mode.
		*/
		template <Operation operation, AddressingMode addressing_mode> void perform();

	private:
		// MARK: - Instruction set state.

		// Memory.
		std::array<uint8_t, 0x2000> memory_;

		// Registers.
		uint8_t a_ = 0, x_ = 0, y_ = 0, s_ = 0;

		uint8_t negative_result_ = 0;
		uint8_t zero_result_ = 0;
		uint8_t interrupt_disable_ = 0x04;
		uint8_t carry_flag_ = 0;
		uint8_t overflow_result_ = 0;
		bool index_mode_ = false;
		bool decimal_mode_ = false;

		// IO ports.
		uint8_t port_directions_[4] = {0x00, 0x00, 0x00, 0x00};
		uint8_t port_outputs_[4] = {0xff, 0xff, 0xff, 0xff};

		// Timers.
		struct Timer {
			uint8_t value = 0xff, reload_value = 0xff;
		};
		int timer_divider_ = 0;
		Timer timers_[3], prescalers_[2];
		inline int update_timer(Timer &timer, int count);

		// Interrupt and timer control.
		uint8_t interrupt_control_ = 0, timer_control_ = 0;
		bool interrupt_line_ = false;

		// Access helpers.
		inline uint8_t read(uint16_t address);
		inline void write(uint16_t address, uint8_t value);
		inline void push(uint8_t value);
		inline uint8_t pull();
		inline void set_flags(uint8_t);
		inline uint8_t flags();
		template<bool is_brk> inline void perform_interrupt(uint16_t vector);
		inline void set_port_output(int port);

		void set_interrupt_request(uint8_t &reg, uint8_t value, uint16_t vector);

		// MARK: - Execution time

		Cycles cycles_;
		Cycles cycles_since_port_handler_;
		PortHandler &port_handler_;
		inline void subtract_duration(int duration);
};

}
