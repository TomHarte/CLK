//
//  68000Mk2Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef _8000Mk2Storage_h
#define _8000Mk2Storage_h

#include "../../../InstructionSets/M68k/Status.hpp"
#include "../../../InstructionSets/M68k/Decoder.hpp"

namespace CPU {
namespace MC68000Mk2 {

struct ProcessorBase {
	enum State: int {
		Reset			= -1,
		Decode	 		= -2,
		WaitForDTACK	= -3,
		FetchOperand	= -4,
	};
	int state_ = State::Reset;

	HalfCycles time_remaining_;
	int post_dtack_state_ = 0;
	int is_supervisor_ = 1;

	InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68000> decoder_;
	InstructionSet::M68k::Preinstruction instruction_;
	uint16_t opcode_;
	uint8_t operand_flags_;

	InstructionSet::M68k::Status status_;
	SlicedInt32 program_counter_;
	SlicedInt32 registers_[16];		// D0–D7 followed by A0–A7.
	SlicedInt32 stack_pointers_[2];

	bool dtack_ = false;
	bool vpa_ = false;
	bool berr_ = false;

	SlicedInt32 prefetch_;
	int operand_ = 0;
};

}
}

#endif /* _8000Mk2Storage_h */
