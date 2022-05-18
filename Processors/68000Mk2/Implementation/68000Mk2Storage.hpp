//
//  68000Mk2Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef _8000Mk2Storage_h
#define _8000Mk2Storage_h

#include "../../../InstructionSets/M68k/Decoder.hpp"
#include "../../../InstructionSets/M68k/Perform.hpp"
#include "../../../InstructionSets/M68k/Status.hpp"

#include <limits>

namespace CPU {
namespace MC68000Mk2 {

struct ProcessorBase: public InstructionSet::M68k::NullFlowController {
	int state_ = std::numeric_limits<int>::min();

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

	/// Storage for a temporary address, which can't be a local because it'll
	/// be used to populate microcycles, which may persist beyond an entry
	/// and exit of run_for (especially between an address announcement, and
	/// a data select).
	uint32_t temporary_address_ = 0;

	// Flow controller... all TODO.
	using Preinstruction = InstructionSet::M68k::Preinstruction;

	template <typename IntT> void did_mulu(IntT) {}
	template <typename IntT> void did_muls(IntT) {}
	void did_chk(bool, bool) {}
	void did_shift(int) {}
	template <bool did_overflow> void did_divu(uint32_t, uint32_t) {}
	template <bool did_overflow> void did_divs(int32_t, int32_t) {}
	void did_bit_op(int) {}
	inline void did_update_status();
	template <typename IntT> void complete_bcc(bool, IntT) {}
	void complete_dbcc(bool, bool, int16_t) {}
	void bsr(uint32_t) {}
	void jsr(uint32_t) {}
	void jmp(uint32_t) {}
	void rtr() {}
	void rte() {}
	void rts() {}
	void stop() {}
	void reset() {}
	void link(Preinstruction, uint32_t) {}
	void unlink(uint32_t &) {}
	void pea(uint32_t) {}
	void move_to_usp(uint32_t) {}
	void move_from_usp(uint32_t &) {}
	void tas(Preinstruction, uint32_t) {}
	template <typename IntT> void movep(Preinstruction, uint32_t, uint32_t) {}
	template <typename IntT> void movem_toM(Preinstruction, uint32_t, uint32_t) {}
	template <typename IntT> void movem_toR(Preinstruction, uint32_t, uint32_t) {}
	template <bool use_current_instruction_pc = true> void raise_exception(int) {}

	// Some microcycles that will be modified as required and used in the main loop;
	// the semantics of a switch statement make in-place declarations awkward and
	// some of these may persist across multiple calls to run_for.
	Microcycle idle{0};

	// Read a program word. All accesses via the program counter are word sized.
	Microcycle read_program_announce {
		Microcycle::Read | Microcycle::NewAddress | Microcycle::IsProgram
	};
	Microcycle read_program {
		Microcycle::Read | Microcycle::SameAddress | Microcycle::SelectWord | Microcycle::IsProgram
	};

	// Read a data word or byte.
	Microcycle read_word_data_announce {
		Microcycle::Read | Microcycle::NewAddress | Microcycle::IsData
	};
	Microcycle read_word_data {
		Microcycle::Read | Microcycle::SameAddress | Microcycle::SelectWord | Microcycle::IsData
	};
	Microcycle read_byte_data {
		Microcycle::Read | Microcycle::SameAddress | Microcycle::SelectByte | Microcycle::IsData
	};

	// Write a data word or byte.
	Microcycle write_word_data_announce {
		Microcycle::NewAddress | Microcycle::IsData
	};
	Microcycle write_word_data {
		Microcycle::SameAddress | Microcycle::SelectWord | Microcycle::IsData
	};
	Microcycle write_byte_data {
		Microcycle::SameAddress | Microcycle::SelectByte | Microcycle::IsData
	};

	// Holding spot when awaiting DTACK/etc.
	Microcycle awaiting_dtack;
};

}
}

#endif /* _8000Mk2Storage_h */
