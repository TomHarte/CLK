//
//  Executor.h
//  Clock Signal
//
//  Created by Thomas Harte on 1/16/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Executor_h
#define Executor_h

#include "Instruction.hpp"

namespace InstructionSet {
namespace M50740 {

class Executor;

class Executor {
	public:
		/*!
			M50740 actions require no further context; the addressing mode and operation is baked in,
			so using the Executor to enquire of memory and the program counter is sufficient.
		*/
		struct Action {
			void (* perform)(Executor *) = nullptr;
		};

		Action action_for(Instruction);

	private:
		/*!
			Performs @c operation using @c operand as the value fetched from memory, if any.
		*/
		template <Operation operation> static void perform(uint8_t *operand);

		/*!
			Performs @c operation in @c addressing_mode.
		*/
		template <Operation operation, AddressingMode addressing_mode> static void perform(Executor *);
};

}
}

#endif /* Executor_h */
