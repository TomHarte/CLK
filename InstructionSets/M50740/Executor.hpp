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

namespace InstructionSet {
namespace M50740 {

class Executor;

class Executor: public CachingExecutor<Executor, 0x2000, 256, false> {
	public:
		Executor();

	private:
		// MARK: - CachingExecutor-facing interface.

		friend CachingExecutor<Executor, 0x2000, 256, false>;

		/*!
			Maps instructions to performers; called by the CachingExecutor and for this instruction set, extremely trivial.
		*/
		inline PerformerIndex action_for(Instruction instruction) {
			// This is a super-simple processor, so the opcode can be used directly to index the performers.
			return instruction.opcode;
		}

	private:
		// MARK: - Internal framework for generator performers.

		/*!
			Provides dynamic lookup of @c perform(Executor*).
		*/
		class PerformerLookup {
			public:
				PerformerLookup() {
					fill<int(MinOperation), int(MinAddressingMode)>(performers);
				}

				Performer performer(Operation operation, AddressingMode addressing_mode) {
					return performers[int(addressing_mode) * (MaxOperation - MinOperation) + int(operation) - MinOperation];
				}

			private:
				Performer performers[(MaxAddressingMode - MinAddressingMode) * (MaxOperation - MinOperation)];

				template<int operation, int addressing_mode> void fill_operation(Performer *target) {
					*target = &Executor::perform<Operation(operation), AddressingMode(addressing_mode)>;
					if constexpr (addressing_mode+1 < MaxAddressingMode) {
						fill_operation<operation, addressing_mode+1>(target + 1);
					}
				}

				template<int operation, int addressing_mode> void fill(Performer *target) {
					fill_operation<operation, addressing_mode>(target);
					target += MaxOperation - MinOperation;
					if constexpr (operation+1 < MaxOperation) {
						fill<operation+1, addressing_mode>(target);
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
		uint8_t a_, x_, y_;
};

}
}

#endif /* Executor_h */
