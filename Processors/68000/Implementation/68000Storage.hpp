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
		uint_fast32_t zero_flag_;		// The zero flag is set if this value is zero.
		uint_fast32_t carry_flag_;		// The carry flag is set if this value is non-zero.
		uint_fast32_t extend_flag_;		// The extend flag is set if this value is non-zero.
		uint_fast32_t overflow_flag_;	// The overflow flag is set if this value is non-zero.
		uint_fast32_t negative_flag_;	// The negative flag is set if this value is non-zero.

		// Generic sources and targets for memory operations;
		// by convention: [0] = source, [1] = destination.
		uint32_t effective_address_[2];
		RegisterPair32 bus_data_[2];

		enum class Operation {
			ABCD,	SBCD,
			ADD,	AND,	EOR,	OR,		SUB,

			MOVEb,	MOVEw,	MOVEl
		};

		/*!
			Bus steps are sequences of things to communicate to the bus.
			Standard behaviour is: (i) perform microcycle; (ii) perform action.
		*/
		struct BusStep {
			Microcycle microcycle;
			enum class Action {
				None,

				/// Performs effective_address_[0] += 2.
				IncrementEffectiveAddress0,

				/// Performs effective_address_[1] += 2.
				IncrementEffectiveAddress1,

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

			inline bool operator ==(const BusStep &rhs) const {
				if(action != rhs.action) return false;
				return microcycle == rhs.microcycle;
			}

			inline bool is_terminal() const {
				return action == Action::ScheduleNextProgram;
			}
		};

		/*!
			A micro-op is: (i) an action to take; and (ii) a sequence of bus operations
			to perform after taking the action.

			NOTE: this therefore has the opposite order of behaviour compared to a BusStep,
			the action occurs BEFORE the bus operations, not after.

			A nullptr bus_program terminates a sequence of micro operations; the is_terminal
			test should be used to query for that. The action on the final operation will
			be performed.
		*/
		struct MicroOp {
			enum class Action: int {
				None,
				PerformOperation,

				/*
					All of the below will honour the source and destination masks
					in deciding where to apply their actions.
				*/

				/// Subtracts 1.
				Decrement1,
				/// Subtracts 2.
				Decrement2,
				/// Subtracts 4.
				Decrement4,

				/// Adds 1.
				Increment1,
				/// Adds 2.
				Increment2,
				/// Adds 4.
				Increment4,

				/// Peeking into the prefetch queue, calculates the proper target of (d16,An) addressing.
				CalcD16An,

				/// Peeking into the prefetch queue, calculates the proper target of (d8,An,Xn) addressing.
				CalcD8AnXn,

				/// Peeking into the prefetch queue, calculates the proper target of (d16,PC) addressing,
				/// adjusting as though it had been performed after the proper PC fetches. The source
				/// and destination mask flags affect only the destination of the result.
				CalcD16PC,

				/// Peeking into the prefetch queue, calculates the proper target of (d8,An,Xn) addressing,
				/// adjusting as though it had been performed after the proper PC fetches. The source
				/// and destination mask flags affect only the destination of the result.
				CalcD8PCXn,

				/// Sets the high word according to the MSB of the low word.
				SignExtendWord,

				/// Sets the high three bytes according to the MSB of the low byte.
				SignExtendByte,
			};
			static const int SourceMask = 1 << 30;
			static const int DestinationMask = 1 << 29;
			int action = int(Action::None);

			BusStep *bus_program = nullptr;

			MicroOp() {}
			MicroOp(int action) : action(action) {}
			MicroOp(int action, BusStep *bus_program) : action(action), bus_program(bus_program) {}

			MicroOp(Action action) : MicroOp(int(action)) {}
			MicroOp(Action action, BusStep *bus_program) : MicroOp(int(action), bus_program) {}

			inline bool is_terminal() const {
				return bus_program == nullptr;
			}
		};

		/*!
			A program represents the implementation of a particular opcode, as a sequence
			of micro-ops and, separately, the operation to perform plus whatever other
			fields the operation requires.
		*/
		struct Program {
			MicroOp *micro_operations = nullptr;
			RegisterPair32 *source = nullptr;
			RegisterPair32 *destination = nullptr;
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

		/// Copies address_[7] to the proper stack pointer based on current mode.
		void write_back_stack_pointer();

		/// Sets or clears the supervisor flag, ensuring the stack pointer is properly updated.
		void set_is_supervisor(bool);

	private:
		friend class ProcessorStorageConstructor;
};

#endif /* MC68000Storage_h */
