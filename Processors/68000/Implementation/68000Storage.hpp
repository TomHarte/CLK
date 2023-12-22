//
//  68000Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef MC68000Storage_h
#define MC68000Storage_h

#include "../../../InstructionSets/M68k/Decoder.hpp"
#include "../../../InstructionSets/M68k/Perform.hpp"
#include "../../../InstructionSets/M68k/Status.hpp"

#include <limits>

namespace CPU::MC68000 {

struct ProcessorBase: public InstructionSet::M68k::NullFlowController {
	ProcessorBase() {
		read_program_announce.address = read_program.address = &program_counter_.l;
	}
	ProcessorBase(const ProcessorBase& rhs) = delete;
	ProcessorBase& operator=(const ProcessorBase& rhs) = delete;

	int state_ = 0;

	/// Counts time left on the clock before the current batch of processing
	/// is complete; may be less than zero.
	HalfCycles time_remaining_;

	/// E clock phase.
	HalfCycles e_clock_phase_;

	/// Current supervisor state, for direct provision to the bus handler.
	int is_supervisor_ = 1;

	// A decoder for instructions, plus all collected information about the
	// current instruction.
	InstructionSet::M68k::Predecoder<InstructionSet::M68k::Model::M68000> decoder_;
	InstructionSet::M68k::Preinstruction instruction_;
	uint16_t opcode_;
	uint8_t operand_flags_;
	SlicedInt32 instruction_address_;

	// Register state.
	InstructionSet::M68k::Status status_;
	SlicedInt32 program_counter_;
	SlicedInt32 registers_[16]{};		// D0–D7 followed by A0–A7.
	SlicedInt32 stack_pointers_[2];

	/// Current state of the DTACK input.
	bool dtack_ = false;
	/// Current state of the VPA input.
	bool vpa_ = false;
	/// Current state of the BERR input.
	bool berr_ = false;
	/// Current input interrupt level.
	int bus_interrupt_level_ = 0;

	// Whether to trace at the end of this instruction.
	InstructionSet::M68k::Status::FlagT should_trace_ = 0;

	// I don't have great information on the 68000 interrupt latency; as a first
	// guess, capture the bus interrupt level upon every prefetch, and use that for
	// the inner-loop decision.
	int captured_interrupt_level_ = 0;

	/// Contains the prefetch queue; the most-recently fetched thing is the
	/// low portion of this word, and the thing fetched before that has
	/// proceeded to the high portion.
	SlicedInt32 prefetch_;

	// Temporary storage for the current instruction's operands
	// and the corresponding effective addresses.
	CPU::SlicedInt32 operand_[2];
	CPU::SlicedInt32 effective_address_[2];

	/// If currently in the wait-for-DTACK state, this indicates where to go
	/// upon receipt of DTACK or VPA. BERR will automatically segue
	/// into the proper exception.
	int post_dtack_state_ = 0;

	/// If using CalcEffectiveAddress, this is the state to adopt after the
	/// effective address for next_operand_ has been calculated.
	int post_ea_state_ = 0;

	/// The perform state for this operation.
	int perform_state_ = 0;

	/// When fetching or storing operands, this is the next one to fetch
	/// or store.
	int next_operand_ = -1;

	/// Storage for a temporary address, which can't be a local because it'll
	/// be used to populate microcycles, which may persist beyond an entry
	/// and exit of run_for (especially between an address announcement, and
	/// a data select).
	SlicedInt32 temporary_address_;

	/// Storage for a temporary value; primarily used by MOVEP to split a 32-bit
	/// source into bus-compatible byte units.
	SlicedInt32 temporary_value_;

	/// A record of the exception to trigger.
	int exception_vector_ = 0;

	/// Transient storage for exception processing.
	SlicedInt16 captured_status_;

	/// An internal flag used during various dynamically-sized instructions
	/// (e.g. BCHG, DIVU) to indicate how much additional processing happened;
	/// this is measured in microcycles.
	int dynamic_instruction_length_ = 0;

	/// Two bits of state for MOVEM, being the curent register and what to
	/// add to it to get to the next register.
	int register_index_ = 0, register_delta_ = 0;

	// A lookup table that aids with effective address calculation in
	// predecrement and postincrement modes; index as [size][register]
	// and note that [0][7] is 2 rather than 1.
	static constexpr uint32_t address_increments[3][8] = {
		{ 1, 1, 1, 1, 1, 1, 1, 2, },
		{ 2, 2, 2, 2, 2, 2, 2, 2, },
		{ 4, 4, 4, 4, 4, 4, 4, 4, },
	};

	// A lookup table that ensures write-back to data registers affects
	// only the correct bits.
	static constexpr uint32_t size_masks[3] = { 0xff, 0xffff, 0xffff'ffff };

	// Assumptions used by the lookup tables above:
	static_assert(int(InstructionSet::M68k::DataSize::Byte) == 0);
	static_assert(int(InstructionSet::M68k::DataSize::Word) == 1);
	static_assert(int(InstructionSet::M68k::DataSize::LongWord) == 2);

	/// Used by some dedicated read-modify-write perform patterns to
	/// determine the size of the bus operation.
	OperationT select_flag_ = 0;

	// Captured bus/address-error state.
	Microcycle<Operation::DecodeDynamically> bus_error_;

	// Flow controller methods implemented.
	using Preinstruction = InstructionSet::M68k::Preinstruction;
	template <typename IntT> void did_mulu(IntT);
	template <typename IntT> void did_muls(IntT);
	inline void did_chk(bool, bool);
	inline void did_scc(bool);
	template <typename IntT> void did_shift(int);
	template <bool did_overflow> void did_divu(uint32_t, uint32_t);
	template <bool did_overflow> void did_divs(int32_t, int32_t);
	inline void did_bit_op(int);
	inline void did_update_status();
	template <typename IntT> void complete_bcc(bool, IntT);
	inline void complete_dbcc(bool, bool, int16_t);
	inline void move_to_usp(uint32_t);
	inline void move_from_usp(uint32_t &);
	inline void tas(Preinstruction, uint32_t);
	template <bool use_current_instruction_pc = true> void raise_exception(int);

	// These aren't implemented because the specific details of the implementation
	// mean that the performer call-out isn't necessary.
	template <typename IntT> void movep(Preinstruction, uint32_t, uint32_t) {}
	template <typename IntT> void movem_toM(Preinstruction, uint32_t, uint32_t) {}
	template <typename IntT> void movem_toR(Preinstruction, uint32_t, uint32_t) {}
	void jsr(uint32_t) {}
	void bsr(uint32_t) {}
	void jmp(uint32_t) {}
	inline void pea(uint32_t) {}
	inline void link(Preinstruction, uint32_t) {}
	inline void unlink(uint32_t &) {}
	inline void rtr() {}
	inline void rte() {}
	inline void rts() {}
	inline void reset() {}
	inline void stop() {}

	// Some microcycles that will be modified as required and used in the main loop;
	// the semantics of a switch statement make in-place declarations awkward and
	// some of these may persist across multiple calls to run_for.
	Microcycle<OperationT(0)> idle;

	// Read a program word. All accesses via the program counter are word sized.
	static constexpr OperationT
		ReadProgramAnnounceOperation = Operation::Read | Operation::NewAddress | Operation::IsProgram;
	static constexpr OperationT
		ReadProgramOperation = Operation::Read | Operation::SameAddress | Operation::SelectWord | Operation::IsProgram;
	Microcycle<ReadProgramAnnounceOperation> read_program_announce{};
	Microcycle<ReadProgramOperation> read_program{};

	// Read a data word or byte.
	Microcycle<Operation::DecodeDynamically> access_announce {
		Operation::Read | Operation::NewAddress | Operation::IsData
	};
	Microcycle<Operation::DecodeDynamically> access {
		Operation::Read | Operation::SameAddress | Operation::SelectWord | Operation::IsData
	};

	// TAS.
	static constexpr OperationT
		TASOperations[5] = {
			Operation::Read | Operation::NewAddress | Operation::IsData,
			Operation::Read | Operation::SameAddress | Operation::IsData | Operation::SelectByte,
			Operation::SameAddress,
			Operation::SameAddress | Operation::IsData,
			Operation::SameAddress | Operation::IsData | Operation::SelectByte,
		};
	Microcycle<Operation::DecodeDynamically> tas_cycles[5] = {
		{ TASOperations[0] },
		{ TASOperations[1] },
		{ TASOperations[2] },
		{ TASOperations[3] },
		{ TASOperations[4] },
	};

	// Reset.
	static constexpr OperationT ResetOperation = CPU::MC68000::Operation::Reset;
	Microcycle<ResetOperation> reset_cycle { HalfCycles(248) };

	// Interrupt acknowledge.
	static constexpr OperationT
		InterruptCycleOperations[2] = {
			Operation::InterruptAcknowledge | Operation::Read | Operation::NewAddress,
			Operation::InterruptAcknowledge | Operation::Read | Operation::SameAddress | Operation::SelectByte
		};
	Microcycle<Operation::DecodeDynamically> interrupt_cycles[2] = {
		{ InterruptCycleOperations[0] },
		{ InterruptCycleOperations[1] },
	};

	// Holding spot when awaiting DTACK/etc.
	Microcycle<Operation::DecodeDynamically> awaiting_dtack;
};

}

#endif /* MC68000Storage_h */
