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
		RegisterPair32 address_[8];
		RegisterPair32 program_counter_;

		RegisterPair32 stack_pointers_[2];	// [0] = user stack pointer; [1] = supervisor; the values from here
											// are copied into/out of address_[7] upon mode switches.

		RegisterPair16 prefetch_queue_[2];
		bool dtack_ = true;

		// Various status bits.
		int is_supervisor_;

		// Generic sources and targets for memory operations.
		uint32_t effective_address_;
		RegisterPair32 bus_data_[2];

		enum class Operation {
			ABCD,	SBCD,
			ADD,	AND,	EOR,	OR,		SUB,
		};

		/*!
			Bus steps are sequences of things to communicate to the bus.
		*/
		struct BusStep {
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

		/*!
			A micro-op is: (i) an action to take; and (ii) a sequence of bus operations
			to perform after taking the action.

			A nullptr bus_program terminates a sequence of micro operations.
		*/
		struct MicroOp {
			enum class Action {
				None,
				PerformOperation,

				PredecrementSourceAndDestination1,
				PredecrementSourceAndDestination2,
				PredecrementSourceAndDestination4,
			} action = Action::None;
			BusStep *bus_program = nullptr;
		};

		/*!
			A program represents the implementation of a particular opcode, as a sequence
			of micro-ops and, separately, the operation to perform plus whatever other
			fields the operation requires.
		*/
		struct Program {
			MicroOp *micro_operations = nullptr;
			RegisterPair32 *source;
			RegisterPair32 *destination;
			Operation operation;
		};

		// Storage for all the sequences of bus steps and micro-ops used throughout
		// the 68000.
		std::vector<BusStep> all_bus_steps_;
		std::vector<MicroOp> all_micro_ops_;

		// A lookup table from instructions to implementations.
		Program instructions[65536];

		// Special programs, for exception handlers.
		BusStep *reset_program_;

		// Current bus step pointer, and outer program pointer.
		Program *active_program_ = nullptr;
		MicroOp *active_micro_op_ = nullptr;
		BusStep *active_step_ = nullptr;

	private:
		/*!
			Installs BusSteps that implement the described program into the relevant
			instance storage, returning the offset within @c all_bus_steps_ at which
			the generated steps begin.

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
		size_t assemble_program(const char *access_pattern, const std::vector<uint32_t *> &addresses = {}, int data_mask = Microcycle::UpperData | Microcycle::LowerData);

		struct BusStepCollection {
			size_t six_step_Dn;
			size_t four_step_Dn;

			// The next two are indexed as [source][destination].
			size_t double_predec_byte[8][8];
			size_t double_predec_word[8][8];
			size_t double_predec_long[8][8];
		};
		BusStepCollection assemble_standard_bus_steps();

		/*!
			Disassembles the instruction @c instruction and inserts it into the
			appropriate lookup tables.
		*/
		void install_instructions(const BusStepCollection &);
};

#endif /* MC68000Storage_h */
