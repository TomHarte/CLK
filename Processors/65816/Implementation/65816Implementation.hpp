//
//  65816Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

template <typename BusHandler> void Processor<BusHandler>::run_for(const Cycles cycles) {
	auto int_cycles = cycles.as_integral();
	while(int_cycles--) {
		const MicroOp operation = *next_op_;
		++next_op_;

		switch(operation) {
			case OperationMoveToNextProgram:
				// The exception program will determine the appropriate way to respond
				// based on the pending exception if one exists; otherwise just do a
				// standard fetch-decode-execute.
				next_op_ = &micro_ops_[&instructions[pending_exceptions_ ? size_t(OperationSlot::Exception) : size_t(OperationSlot::FetchDecodeExecute)].program_offset];

				// TODO: reset instruction buffer.
			continue;

			default:
				assert(false);
		}
	}
}
