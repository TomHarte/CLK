//
//  68000Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

template <class T, bool dtack_is_implicit> void Processor<T, dtack_is_implicit>::run_for(Cycles cycles) {
	// TODO: obey the 'cycles' count.
	while(true) {
		// Check whether the program is exhausted.
		if(active_program_->action == Step::Action::ScheduleNextProgram) {
			std::cerr << "68000 Abilities exhausted" << std::endl;
			return;
		}

		// The program is not exhausted, so perform the microcycle.

		// Check for DTack if this isn't being treated implicitly.
		if(!dtack_is_implicit) {
			if(active_program_->microcycle.operation & (Microcycle::UpperData | Microcycle::LowerData) && !dtack_) {
				// TODO: perform wait state.
				continue;
			}
		}

		// Perform the microcycle.
		bus_handler_.perform_bus_operation(active_program_->microcycle, is_supervisor_);

		// Perform the post-hoc action.
		switch(active_program_->action) {
			default:
				std::cerr << "Unimplemented 68000 action: " << int(active_program_->action) << std::endl;
				return;
			break;

			case Step::Action::None: break;

			case Step::Action::IncrementEffectiveAddress:	effective_address_ += 2;	break;
			case Step::Action::IncrementProgramCounter:		program_counter_.full += 2;	break;

			case Step::Action::AdvancePrefetch:
				prefetch_queue_[0] = prefetch_queue_[1];
			break;
		}
	}
}
