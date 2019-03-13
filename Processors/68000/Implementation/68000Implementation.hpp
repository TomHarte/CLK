//
//  68000Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

template <class T, bool dtack_is_implicit> void Processor<T, dtack_is_implicit>::run_for(HalfCycles duration) {
	// TODO: obey the 'cycles' count.
	while(true) {
		// Check whether the current list of bus steps is exhausted; if so then
		// seek out another one from the current program (if any), and if there
		// are no more to do, revert to scheduling something else (after checking
		// for interrupts).
		if(active_step_->action == BusStep::Action::ScheduleNextProgram) {
			if(active_micro_op_) {
				++active_micro_op_;
				switch(active_micro_op_->action) {
					case MicroOp::Action::None: break;

					case MicroOp::Action::PerformOperation:
						std::cerr << "Should do something with program operation " << int(active_program_->operation) << std::endl;
					break;
				}
				active_step_ = active_micro_op_->bus_program;
			}

			if(!active_step_) {
				std::cerr << "68000 Abilities exhausted; should schedule an instruction or something?" << std::endl;
				return;
			}
		}

		// The bus step list is not exhausted, so perform the microcycle.

		// Check for DTack if this isn't being treated implicitly.
		if(!dtack_is_implicit) {
			if(active_step_->microcycle.operation & (Microcycle::UpperData | Microcycle::LowerData) && !dtack_) {
				// TODO: perform wait state.
				continue;
			}
		}

		// TODO: synchronous bus.

		// Perform the microcycle.
		bus_handler_.perform_bus_operation(active_step_->microcycle, is_supervisor_);

		// Perform the post-hoc action.
		switch(active_step_->action) {
			default:
				std::cerr << "Unimplemented 68000 bus step action: " << int(active_step_->action) << std::endl;
				return;
			break;

			case BusStep::Action::None: break;

			case BusStep::Action::IncrementEffectiveAddress:	effective_address_ += 2;	break;
			case BusStep::Action::IncrementProgramCounter:		program_counter_.full += 2;	break;

			case BusStep::Action::AdvancePrefetch:
				prefetch_queue_[0] = prefetch_queue_[1];
			break;
		}

		// Move to the next program step.
		++active_step_;
	}
}
