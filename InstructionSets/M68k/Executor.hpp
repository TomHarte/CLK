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
#include "Sequence.hpp"
#include "Status.hpp"

namespace InstructionSet {
namespace M68k {

struct BusHandler {
	template <typename IntT> void write(uint32_t address, IntT value);
	template <typename IntT> IntT read(uint32_t address);
};

/// Ties together the decoder, sequencer and performer to provide an executor for 680x0 instruction streams.
/// As is standard for these executors, no bus- or cache-level fidelity to any real 680x0 is attempted. This is
/// simply an executor of 680x0 code.
template <Model model, typename BusHandler> class Executor {
	public:
		Executor(BusHandler &);

		/// Executes the number of instructions specified;
		/// other events — such as initial reset or branching
		/// to exceptions — may be zero costed, and interrupts
		/// will not necessarily take effect immediately when signalled.
		void run_for_instructions(int);

	private:
		BusHandler &bus_handler_;
		Predecoder<model> decoder_;

		void reset();

		// Processor state.
		Status status_;
		CPU::SlicedInt32 program_counter_;
};

}
}

#endif /* InstructionSets_M68k_Executor_hpp */
