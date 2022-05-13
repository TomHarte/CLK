//
//  Executor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_Executor_hpp
#define InstructionSets_M68k_Executor_hpp

#include "Decoder.hpp"
#include "Instruction.hpp"
#include "Model.hpp"
#include "Perform.hpp"
#include "Status.hpp"

namespace InstructionSet {
namespace M68k {

/// Maps the 68k function codes such that bits 0, 1 and 2 represent
/// FC0, FC1 and FC2 respectively.
enum class FunctionCode {
	UserData 				= 0b001,
	UserProgram				= 0b010,
	SupervisorData			= 0b101,
	SupervisorProgram		= 0b110,
	InterruptAcknowledge	= 0b111,
};

/// The Executor is templated on a class that implements bus handling as defined below;
/// the bus handler is responsible for all reads and writes, and will also receive resets and
/// interrupt acknowledgements.
///
/// The executor will provide 32-bit addresses and act as if it had a 32-bit data bus, even
/// if interpretting the original 68000 instruction set.
struct BusHandler {
	/// Write @c value of type/size @c IntT to @c address with the processor signalling
	/// a FunctionCode of @c function. @c IntT will be one of @c uint8_t, @c uint16_t
	/// or @c uint32_t.
	template <typename IntT> void write(uint32_t address, IntT value, FunctionCode function);

	/// Read and return a value of type/size @c IntT from @c address with the processor signalling
	/// a FunctionCode of @c function. @c IntT will be one of @c uint8_t, @c uint16_t
	/// or @c uint32_t.
	template <typename IntT> IntT read(uint32_t address, FunctionCode function);

	/// React to the processor programmatically strobing its RESET output.
	void reset();

	/// Respond to an interrupt acknowledgement at @c interrupt_level from the processor.
	/// Should return @c -1 in order to trigger autovectoring, or the appropriate exception vector
	/// number otherwise.
	///
	/// It is undefined behaviour to return a number greater than 255.
	int acknowlege_interrupt(int interrupt_level);
};

/// Ties together the decoder, sequencer and performer to provide an executor for 680x0 instruction streams.
/// As is standard for these executors, no bus- or cache-level fidelity to any real 680x0 is attempted. This is
/// simply an executor of 680x0 code.
template <Model model, typename BusHandler> class Executor: public NullFlowController {
	public:
		Executor(BusHandler &);

		/// Executes the number of instructions specified;
		/// other events — such as initial reset or branching
		/// to exceptions — may be zero costed, and interrupts
		/// will not necessarily take effect immediately when signalled.
		void run_for_instructions(int);

		/// Call this at any time to interrupt processing with a bus error;
		/// the function code and address must be provided. Internally
		/// this will raise a C++ exception, and therefore doesn't return.
		[[noreturn]] void signal_bus_error(FunctionCode, uint32_t address);

		/// Sets the current input interrupt level.
		void set_interrupt_level(int);

		// Flow control; Cf. Perform.hpp.
		template <bool use_current_instruction_pc = true> void raise_exception(int);

		void did_update_status();

		template <typename IntT> void complete_bcc(bool matched_condition, IntT offset);
		void complete_dbcc(bool matched_condition, bool overflowed, int16_t offset);
		void bsr(uint32_t offset);
		void jmp(uint32_t);
		void jsr(uint32_t offset);
		void rtr();
		void rts();
		void rte();
		void stop();
		void reset();

		void link(Preinstruction instruction, uint32_t offset);
		void unlink(uint32_t &address);
		void pea(uint32_t address);

		void move_to_usp(uint32_t address);
		void move_from_usp(uint32_t &address);

		template <typename IntT> void movep(Preinstruction instruction, uint32_t source, uint32_t dest);
		template <typename IntT> void movem_toM(Preinstruction instruction, uint32_t source, uint32_t dest);
		template <typename IntT> void movem_toR(Preinstruction instruction, uint32_t source, uint32_t dest);

		void tas(Preinstruction instruction, uint32_t address);

		// TODO: ownership of this shouldn't be here.
		struct Registers {
			uint32_t data[8], address[7];
			uint32_t user_stack_pointer;
			uint32_t supervisor_stack_pointer;
			uint16_t status;
			uint32_t program_counter;
		};
		Registers get_state();
		void set_state(const Registers &);

	private:
		void run(int &);

		BusHandler &bus_handler_;
		Predecoder<model> decoder_;

		void reset_processor();
		struct EffectiveAddress {
			CPU::SlicedInt32 value;
			bool requires_fetch;
		};
		EffectiveAddress calculate_effective_address(Preinstruction instruction, uint16_t opcode, int index);

		void read(DataSize size, uint32_t address, CPU::SlicedInt32 &value);
		void write(DataSize size, uint32_t address, CPU::SlicedInt32 value);
		template <typename IntT> IntT read(uint32_t address, bool is_from_pc = false);
		template <typename IntT> void write(uint32_t address, IntT value);

		template <typename IntT> IntT read_pc();

		uint32_t index_8bitdisplacement();

		// Processor state.
		Status status_;
		CPU::SlicedInt32 program_counter_;
		CPU::SlicedInt32 registers_[16];	// D0–D8, followed by A0–A8.
		CPU::SlicedInt32 stack_pointers_[2];
		uint32_t instruction_address_;
		uint16_t instruction_opcode_;
		int active_stack_pointer_ = 0;

		// Bus state.
		int interrupt_input_ = 0;

		// A lookup table to ensure that A7 is adjusted by 2 rather than 1 in
		// postincrement and predecrement mode.
		static constexpr uint32_t byte_increments[] = {
			1, 1, 1, 1, 1, 1, 1, 2
		};
};

}
}

#include "Implementation/ExecutorImplementation.hpp"

#endif /* InstructionSets_M68k_Executor_hpp */
