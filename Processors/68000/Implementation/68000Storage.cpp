//
//  68000Storage.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "../68000.hpp"

using namespace CPU::MC68000;

ProcessorStorage::ProcessorStorage() {
	reset_program_ = assemble_program("n- n- n- n- n- nn nF nf nV nv np np");

	// Set initial state. Largely TODO.
	active_program_ = reset_program_.data();
	effective_address_ = 0;
	is_supervisor_ = 1;
}

// TODO: allow actions to be specified, of course.
std::vector<ProcessorStorage::Step> ProcessorStorage::assemble_program(const char *access_pattern) {
	std::vector<Step> steps;

	// Parse the access pattern to build microcycles.
	while(*access_pattern) {
		Step step;

		switch(*access_pattern) {
			case '\t': case ' ': // White space acts as a no-op; it's for clarity only.
				++access_pattern;
			break;

			case 'n':	// This might be a plain NOP cycle, in which some internal calculation occurs,
						// or it might pair off with something afterwards.
				switch(access_pattern[1]) {
					default:	// This is probably a pure NOP; if what comes after this 'n' isn't actually
								// valid, it should be caught in the outer switch the next time around the loop.
						steps.push_back(step);
						++access_pattern;
					break;

					case '-':	// This is two NOPs in a row.
						steps.push_back(step);
						steps.push_back(step);
						access_pattern += 2;
					break;

					case 'F':	// Fetch SSP MSW.
					case 'f':	// Fetch SSP LSW.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;	// IsProgram is a guess.
						step.microcycle.address = &effective_address_;
						step.microcycle.value = isupper(access_pattern[1]) ? &stack_pointers_[1].halves.high : &stack_pointers_[1].halves.low;
						steps.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = Step::Action::IncrementEffectiveAddress;
						steps.push_back(step);

						access_pattern += 2;
					break;

					case 'V':	// Fetch exception vector low.
					case 'v':	// Fetch exception vector high.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;	// IsProgram is a guess.
						step.microcycle.address = &effective_address_;
						step.microcycle.value = isupper(access_pattern[1]) ? &program_counter_.halves.high : &program_counter_.halves.low;
						steps.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = Step::Action::IncrementEffectiveAddress;
						steps.push_back(step);

						access_pattern += 2;
					break;

					case 'p':	// Fetch from the program counter into the prefetch queue.
						step.microcycle.length = HalfCycles(5);
						step.microcycle.operation = Microcycle::Address | Microcycle::ReadWrite | Microcycle::IsProgram;
						step.microcycle.address = &program_counter_.full;
						step.microcycle.value = &prefetch_queue_[1];
						step.action = Step::Action::AdvancePrefetch;
						steps.push_back(step);

						step.microcycle.length = HalfCycles(3);
						step.microcycle.operation |= Microcycle::LowerData | Microcycle::UpperData;
						step.action = Step::Action::IncrementProgramCounter;
						steps.push_back(step);

						access_pattern += 2;
					break;
				}
			break;

			default:
				std::cerr << "MC68000 program builder; Unknown access type " << *access_pattern << std::endl;
				assert(false);
		}
	}

	// Add a final 'ScheduleNextProgram' sentinel.
	Step end_program;
	end_program.action = Step::Action::ScheduleNextProgram;
	steps.push_back(end_program);

	return steps;
}
