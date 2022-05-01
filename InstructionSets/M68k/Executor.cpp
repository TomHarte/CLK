//
//  Executor.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "Executor.hpp"

#include "Perform.hpp"

#include <cassert>

using namespace InstructionSet::M68k;

template <Model model, typename BusHandler>
Executor<model, BusHandler>::Executor(BusHandler &handler) : bus_handler_(handler) {
	reset();
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::reset() {
	// TODO.
}

template <Model model, typename BusHandler>
void Executor<model, BusHandler>::run_for_instructions(int count) {
	while(count--) {
		// TODO: check interrupt level, trace flag.

		// Read the next instruction.
		const Preinstruction instruction = decoder_.decode(bus_handler_.template read<uint16_t>(program_counter_.l));
		const auto instruction_address = program_counter_.l;
		program_counter_.l += 2;

		// Obtain the appropriate sequence.
		Sequence<model> sequence(instruction.operation);

		// Temporary storage.
		CPU::SlicedInt32 source, dest;

		// Perform it.
		while(!sequence.empty()) {
			const auto step = sequence.pop_front();

			switch(step) {
				default: assert(false);	// i.e. TODO

				case Step::Perform:
					perform<model>(instruction, source, dest, status_, this);
				break;
			}
		}
	}
}
