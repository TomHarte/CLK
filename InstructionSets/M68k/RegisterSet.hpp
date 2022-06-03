//
//  RegisterSet.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_RegisterSet_h
#define InstructionSets_M68k_RegisterSet_h

namespace InstructionSet {
namespace M68k {

struct RegisterSet {
	uint32_t data[8], address[7];
	uint32_t user_stack_pointer;
	uint32_t supervisor_stack_pointer;
	uint16_t status;
	uint32_t program_counter;

	/// @returns The active stack pointer, whichever it may be.
	uint32_t stack_pointer() const {
		return (status & 0x2000) ? supervisor_stack_pointer : user_stack_pointer;
	}
};

}
}

#endif /* InstructionSets_M68k_RegisterSet_h */
