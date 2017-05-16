//
//  MicroOpScheduler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef MicroOpScheduler_hpp
#define MicroOpScheduler_hpp

namespace CPU {

template <class T> class MicroOpScheduler {
	public:
		MicroOpScheduler() :
			scheduled_programs_{nullptr, nullptr, nullptr, nullptr},
			schedule_programs_write_pointer_(0),
			schedule_programs_read_pointer_(0),
			schedule_program_program_counter_(0) {}

	protected:
		/*
			Up to four programs can be scheduled; each will be carried out in turn. This
			storage maintains pointers to the scheduled list of programs.

			Programs should be terminated by an OperationMoveToNextProgram, causing this
			queue to take that step.
		*/
		const T *scheduled_programs_[4];
		unsigned int schedule_programs_write_pointer_, schedule_programs_read_pointer_, schedule_program_program_counter_;

		/*!
			Schedules a new program, adding it to the end of the queue. Programs should be
			terminated with a OperationMoveToNextProgram. No attempt to copy the program
			is made; a non-owning reference is kept.

			@param program The program to schedule.
		*/
		inline void schedule_program(const T *program) {
			scheduled_programs_[schedule_programs_write_pointer_] = program;
			schedule_programs_write_pointer_ = (schedule_programs_write_pointer_+1)&3;
		}

		inline void move_to_next_program() {
			scheduled_programs_[schedule_programs_read_pointer_] = NULL;
			schedule_programs_read_pointer_ = (schedule_programs_read_pointer_+1)&3;
			schedule_program_program_counter_ = 0;
		}
};

}

#endif /* MicroOpScheduler_hpp */
