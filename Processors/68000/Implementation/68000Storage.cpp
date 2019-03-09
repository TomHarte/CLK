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
	reset_program_ = assemble_program("n- n- n- n- n- nn nF nV nv np np");
}

std::vector<ProcessorStorage::Step> ProcessorStorage::assemble_program(const char *access_pattern) {
	std::vector<Step> steps;

	// Parse the access pattern to build microcycles.
	while(*access_pattern) {
		Step step;

		switch(*access_pattern) {
			case ' ': break;	// Space acts as a no-op; it's for clarity only.

			case 'n':	// Nothing occurs; supply as 'None'.
				step.microcycle.operation = Microcycle::Operation::None;
			break;

			case '-':	// An idle cycle; distinct from a 'None'.
				step.microcycle.operation = Microcycle::Operation::Idle;
			break;

			default: assert(false);
		}

		steps.push_back(step);
		++access_pattern;
	}

	// TODO: add actions, somehow.

	return steps;
}
