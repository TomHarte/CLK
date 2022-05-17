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
	/// States for the state machine which are named by
	/// me for their purpose rather than automatically by file position.
	/// These are negative to avoid ambiguity with the other group.
	enum State: int {
		Reset			= -1,
		Decode	 		= -2,
		WaitForDTACK	= -3,
		FetchOperand	= -4,

		// Various different effective address calculations.

		CalculateAnDn	= -5,

		// Various forms of perform; each of these will
		// perform the current instruction, then do the
		// indicated bus cycle.

		Perform_np		= -6,
	};
	int state_ = State::Reset;

	/// Counts time left on the clock before the current batch of processing
	/// is complete; may be less than zero.
	HalfCycles time_remaining_;

	/// Current supervisor state, for direct provision to the bus handler.
	int is_supervisor_ = 1;

	// A decoder for instructions, plus all collected information about the
	// current instruction.
	InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68000> decoder_;
	InstructionSet::M68k::Preinstruction instruction_;
	uint16_t opcode_;
	uint8_t operand_flags_;
	uint32_t instruction_address_;

	// Register state.
	InstructionSet::M68k::Status status_;
	SlicedInt32 program_counter_;
	SlicedInt32 registers_[16];		// D0–D7 followed by A0–A7.
	SlicedInt32 stack_pointers_[2];

	/// Current state of the DTACK input.
	bool dtack_ = false;
	/// Current state of the VPA input.
	bool vpa_ = false;
	/// Current state of the BERR input.
	bool berr_ = false;

	/// Contains the prefetch queue; the most-recently fetched thing is the
	/// low portion of this word, and the thing fetched before that has
	/// proceeded to the high portion.
	SlicedInt32 prefetch_;

	// Temporary storage for the current instruction's operands
	// and the corresponding effective addresses.
	CPU::SlicedInt32 operand_[2];
	uint32_t effective_address_[2];

	/// If currently in the wait-for-DTACK state, this indicates where to go
	/// upon receipt of DTACK or VPA. BERR will automatically segue
	/// into the proper exception.
	int post_dtack_state_ = 0;

	/// The perform state for this operation.
	int perform_state_ = 0;

	/// When fetching or storing operands, this is the next one to fetch
	/// or store.
	int next_operand_ = 0;
};

}
}

#endif /* _8000Mk2Storage_h */
