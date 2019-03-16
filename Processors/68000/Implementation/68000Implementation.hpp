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
				switch(active_micro_op_->action) {
					case MicroOp::Action::None: break;

					case MicroOp::Action::PerformOperation:
						switch(active_program_->operation) {
							case Operation::ABCD: {
								// Pull out the two halves, for simplicity.
								const uint8_t source = active_program_->source->halves.low.halves.low;
								const uint8_t destination = active_program_->destination->halves.low.halves.low;

								// Perform the BCD add by evaluating the two nibbles separately.
								int result = (destination & 0xf) + (source & 0xf) + (extend_flag_ ? 1 : 0);
								if(result > 0x9) result += 0x06;
								result += (destination & 0xf0) + (source & 0xf0);
								if(result > 0x90) result += 0x60;

								// Set all flags essentially as if this were normal addition.
								zero_flag_ |= result & 0xff;
								extend_flag_ = carry_flag_ = result & ~0xff;
								negative_flag_ = result & 0x80;
								overflow_flag_ = ~(source ^ destination) & (destination ^ result) & 0x80;

								// Store the result.
								active_program_->destination->halves.low.halves.low = uint8_t(result);
							} break;

							case Operation::SBCD: {
								// Pull out the two halves, for simplicity.
								const uint8_t source = active_program_->source->halves.low.halves.low;
								const uint8_t destination = active_program_->destination->halves.low.halves.low;

								// Perform the BCD add by evaluating the two nibbles separately.
								int result = (destination & 0xf) - (source & 0xf) - (extend_flag_ ? 1 : 0);
								if(result > 0x9) result -= 0x06;
								result += (destination & 0xf0) - (source & 0xf0);
								if(result > 0x90) result -= 0x60;

								// Set all flags essentially as if this were normal subtraction.
								zero_flag_ |= result & 0xff;
								extend_flag_ = carry_flag_ = result & ~0xff;
								negative_flag_ = result & 0x80;
								overflow_flag_ = (source ^ destination) & (destination ^ result) & 0x80;

								// Store the result.
								active_program_->destination->halves.low.halves.low = uint8_t(result);
							} break;

							case Operation::MOVEb:
								zero_flag_ = active_program_->destination->halves.low.halves.low = active_program_->source->halves.low.halves.low;
								negative_flag_ = zero_flag_ & 0x80;
							break;

							case Operation::MOVEw:
								zero_flag_ = active_program_->destination->halves.low.full = active_program_->source->halves.low.full;
								negative_flag_ = zero_flag_ & 0x8000;
							break;

							case Operation::MOVEl:
								zero_flag_ = active_program_->destination->full = active_program_->source->full;
								negative_flag_ = zero_flag_ & 0x80000000;
							break;

							default:
								std::cerr << "Should do something with program operation " << int(active_program_->operation) << std::endl;
							break;
						}
					break;

					case MicroOp::Action::PredecrementSourceAndDestination1:
						-- active_program_->source->full;
						-- active_program_->destination->full;
					break;

					case MicroOp::Action::PredecrementSourceAndDestination2:
						active_program_->source->full -= 2;
						active_program_->destination->full -= 2;
					break;

					case MicroOp::Action::PredecrementSourceAndDestination4:
						active_program_->source->full -= 4;
						active_program_->destination->full -= 4;
					break;
				}
			}

			if(active_micro_op_) {
				++active_micro_op_;
				active_step_ = active_micro_op_->bus_program;
			}

			if(!active_step_ || !active_micro_op_) {
				const uint16_t next_instruction = prefetch_queue_[0].full;
				if(!instructions[next_instruction].micro_operations) {
					std::cerr << "68000 Abilities exhausted; should schedule an instruction or something?" << std::endl;
					return;
				}

				active_program_ = &instructions[next_instruction];
				active_micro_op_ = active_program_->micro_operations;
				active_step_ = active_micro_op_->bus_program;
			}
		}

		// The bus step list is not exhausted, so perform the microcycle.

		// Check for DTack if this isn't being treated implicitly.
		if(!dtack_is_implicit) {
			if(active_step_->microcycle.data_select_active() && !dtack_) {
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
