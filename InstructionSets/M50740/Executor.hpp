//
//  Executor.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Executor_h
#define Executor_h

#include "Instruction.hpp"
#include "Parser.hpp"
#include "../CachingExecutor.hpp"
#include "../../ClockReceiver/ClockReceiver.hpp"

#include <cstdint>
#include <vector>

namespace InstructionSet {
namespace M50740 {

class Executor;
using CachingExecutor = CachingExecutor<Executor, 0x1fff, 255, Instruction, false>;

struct PortHandler {
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
			parser.parse(*this, memory_, start & 0x1fff, closing_bound);
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
		uint8_t memory_[0x2000];

		// Registers.
		uint8_t a_, x_, y_, s_;

		uint8_t negative_result_ = 0;
		uint8_t zero_result_ = 0;
		uint8_t interrupt_disable_ = 0;
		uint8_t carry_flag_ = 0;
		uint8_t overflow_result_;
		bool index_mode_ = false;
		bool decimal_mode_ = false;

		inline uint8_t read(uint16_t address);
		inline void write(uint16_t address, uint8_t value);
		inline void push(uint8_t value);
		inline uint8_t pull();
		inline void set_flags(uint8_t);
		inline uint8_t flags();
		template<bool is_brk> inline void perform_interrupt();

		Cycles cycles_;
		PortHandler &port_handler_;
};

}
}

#endif /* Executor_h */
