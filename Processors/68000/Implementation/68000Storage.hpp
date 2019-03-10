//
//  68000Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef MC68000Storage_h
#define MC68000Storage_h

class ProcessorStorage {
	public:
		ProcessorStorage();

	protected:
		RegisterPair32 data_[8];
		RegisterPair32 address_[7];
		RegisterPair32 stack_pointers_[2];	// [0] = user stack pointer; [1] = supervisor
		RegisterPair32 program_counter_;

		RegisterPair16 prefetch_queue_[2];

		enum class State {
			Reset,
			Normal
		};

		// Generic sources and targets for memory operations.
		uint32_t effective_address_;
		RegisterPair32 bus_data_;

		/*!
			A step is a microcycle to perform plus an action to occur afterwards, if any.
		*/
		struct Step {
			Microcycle microcycle;
			enum class Action {
				None,

				/// Performs effective_address_ += 2.
				IncrementEffectiveAddress,

				/// Performs program_counter_ += 2.
				IncrementProgramCounter,

				/// Copies prefetch_queue_[1] to prefetch_queue_[0].
				AdvancePrefetch,

				/*!
					Terminates an atomic program; if nothing else is pending, schedules the next instruction.
					This action is special in that it usurps any included microcycle. So any Step with this
					as its action acts as an end-of-list sentinel.
				*/
				ScheduleNextProgram

			} action = Action::None;
		};

		// Special programs.
		std::vector<Step> reset_program_;

		// Current program pointer.
		Step *active_program_ = nullptr;

	private:
		enum class DataSize {
			Byte, Word, LongWord
		};
		enum class AddressingMode {
		};

		/*!
			Produces a vector of Steps that implement the described program.

			@param access_pattern A string describing the bus activity that occurs
				during this program. This should follow the same general pattern as
				those in yacht.txt; full description below.

			@discussion
			The access pattern is defined, as in yacht.txt, to be a string consisting
			of the following discrete bus actions. Spaces are ignored.

			* n: no operation; data bus is not used;
			* -: idle state; data bus is not used but is also not available;
			* p: program fetch; reads from the PC and adds two to it;
			* W: write MSW of something onto the bus;
			* w: write LSW of something onto the bus;
			* R: read MSW of something from the bus;
			* r: read LSW of soemthing from the bus;
			* S: push the MSW of something onto the stack;
			* s: push the LSW of something onto the stack;
			* U: pop the MSW of something from the stack;
			* u: pop the LSW of something from the stack;
			* V: fetch a vector's MSW;
			* v: fetch a vector's LSW;
			* i: acquire interrupt vector in an IACK cycle;
			* F: fetch the SSPs MSW;
			* f: fetch the SSP's LSW.

			Quite a lot of that is duplicative, implying both something about internal
			state and something about what's observable on the bus, but it's helpful to
			stick to that document's coding exactly for easier debugging.

			p fetches will fill the prefetch queue, attaching an action to both the
			step that precedes them and to themselves. The SSP fetches will go straight
			to the SSP.

			Other actions will by default act via effective_address_ and bus_data_.
			The user should fill in the steps necessary to get data into or extract
			data from those.
		*/
		std::vector<Step> assemble_program(const char *access_pattern);
};

#endif /* MC68000Storage_h */
