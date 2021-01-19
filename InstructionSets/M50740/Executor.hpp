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

#include <cstdint>
#include <vector>

namespace InstructionSet {
namespace M50740 {

class Executor;
using CachingExecutor = CachingExecutor<Executor, 0x1fff, 255, Instruction, false>;

class Executor: public CachingExecutor {
	public:
		Executor();
		void set_rom(const std::vector<uint8_t> &rom);
		void reset();

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
			parser.parse(*this, memory_, start, closing_bound);
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

		void set_program_counter(uint16_t address);

	private:
		// MARK: - Instruction set state.

		// Memory.
		uint8_t memory_[0x2000];

		// Registers.
		uint16_t program_counter_;
		uint8_t a_, x_, y_;
};

}
}

#endif /* Executor_h */
