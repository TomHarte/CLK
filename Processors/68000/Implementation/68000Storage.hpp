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

		RegisterPair32 prefetch_queue_;		// Each word will go into the low part of the word, then proceed upward.
		bool dtack_ = true;

		// Various status bits.
		int is_supervisor_;
		int interrupt_level_;
		uint_fast32_t zero_result_;		// The zero flag is set if this value is zero.
		uint_fast32_t carry_flag_;		// The carry flag is set if this value is non-zero.
		uint_fast32_t extend_flag_;		// The extend flag is set if this value is non-zero.
		uint_fast32_t overflow_flag_;	// The overflow flag is set if this value is non-zero.
		uint_fast32_t negative_flag_;	// The negative flag is set if this value is non-zero.
		uint_fast32_t trace_flag_;		// The trace flag is set if this value is non-zero.

		// Generic sources and targets for memory operations;
		// by convention: [0] = source, [1] = destination.
		RegisterPair32 effective_address_[2];
		RegisterPair32 source_bus_data_[1];
		RegisterPair32 destination_bus_data_[1];

		HalfCycles half_cycles_left_to_run_;

		enum class Operation {
			None,
			ABCD,	SBCD,

			ADDb,	ADDw,	ADDl,
			ADDQb,	ADDQw,	ADDQl,
			ADDAw,	ADDAl,
			ADDQAw,	ADDQAl,

			SUBb,	SUBw,	SUBl,
			SUBQb,	SUBQw,	SUBQl,
			SUBAw,	SUBAl,
			SUBQAw,	SUBQAl,

			MOVEb,	MOVEw,	MOVEl,	MOVEq,
			MOVEAw,	MOVEAl,

			MOVEtoSR, MOVEfromSR,
			MOVEtoCCR,

			ORItoSR,	ORItoCCR,
			ANDItoSR,	ANDItoCCR,
			EORItoSR,	EORItoCCR,

			BTSTb,	BTSTl,
			BCLRl,	BCLRb,
			CMPb,	CMPw,	CMPl,
			TSTb,	TSTw,	TSTl,

			JMP,
			BRA,	Bcc,
			DBcc,
			Scc,

			CLRb, CLRw, CLRl,
			NEGXb, NEGXw, NEGXl,
			NEGb, NEGw, NEGl,

			ASLb, ASLw, ASLl, ASLm,
			ASRb, ASRw, ASRl, ASRm,
			LSLb, LSLw, LSLl, LSLm,
			LSRb, LSRw, LSRl, LSRm,
			ROLb, ROLw, ROLl, ROLm,
			RORb, RORw, RORl, RORm,
			ROXLb, ROXLw, ROXLl, ROXLm,
			ROXRb, ROXRw, ROXRl, ROXRm,

			MOVEMtoRl, MOVEMtoRw,
			MOVEMtoMl, MOVEMtoMw,

			ANDb,	ANDw,	ANDl,
			EORb,	EORw,	EORl,
			NOTb, 	NOTw, 	NOTl,
			ORb,	ORw,	ORl,

			MULU,	MULS,

			RTE_RTR,

			TRAP,

			EXG,	SWAP,

			BCHGl,	BCHGb,
			BSETl,	BSETb,

			TAS,
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

				/// Performs effective_address_[0] -= 2.
				DecrementEffectiveAddress0,

				/// Performs effective_address_[1] -= 2.
				DecrementEffectiveAddress1,

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

				/// Does whatever this instruction says is the main operation.
				PerformOperation,

				/*
					All of the below will honour the source and destination masks
					in deciding where to apply their actions.
				*/

				/// Subtracts 1 from the [source/destination]_address.
				Decrement1,
				/// Subtracts 2 from the [source/destination]_address.
				Decrement2,
				/// Subtracts 4 from the [source/destination]_address.
				Decrement4,

				/// Adds 1 from the [source/destination]_address.
				Increment1,
				/// Adds 2 from the [source/destination]_address.
				Increment2,
				/// Adds 4 from the [source/destination]_address.
				Increment4,

				/// Copies the source and/or destination to effective_address_.
				CopyToEffectiveAddress,

				/// Peeking into the end of the prefetch queue, calculates the proper target of (d16,An) addressing.
				CalcD16An,

				/// Peeking into the end of the prefetch queue, calculates the proper target of (d8,An,Xn) addressing.
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

				/// From the next word in the prefetch queue assembles a 0-padded 32-bit long word in either or
				/// both of effective_address_[0] and effective_address_[1].
				AssembleWordAddressFromPrefetch,

				/// From the next word in the prefetch queue assembles a 0-padded 32-bit long word in either or
				/// both of bus_data_[0] and bus_data_[1].
				AssembleWordDataFromPrefetch,

				/// Copies the next two prefetch words into one of the effective_address_.
				AssembleLongWordAddressFromPrefetch,

				/// Copies the next two prefetch words into one of the bus_data_.
				AssembleLongWordDataFromPrefetch,

				/// Copies the low part of the prefetch queue into next_word_.
				CopyNextWord,

				/// Performs write-back of post-increment address and/or sign extensions as necessary.
				MOVEMtoRComplete,

				/// Performs write-back of pre-decrement address.
				MOVEMtoMComplete,

				// (i) inspects the prefetch queue to determine the length of this instruction and copies the next PC to destination_bus_data_;
				// (ii) copies the stack pointer minus 4 to effective_address_[1];
				// (iii) decrements the stack pointer by four.
				PrepareJSR,
				PrepareBSR,

				// (i) copies the stack pointer to effective_address_[0];
				// (ii) increments the stack pointer by four.
				PrepareRTS,

				// (i) fills in the proper stack addresses to the bus steps for this micro-op; and
				// (ii) adjusts the stack pointer appropriately.
				PrepareRTE_RTR,
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
			RegisterPair32 *source_address = nullptr;
			RegisterPair32 *destination_address = nullptr;
			Operation operation;
			bool requires_supervisor = false;

			void set_source(ProcessorStorage &storage, int mode, int reg) {
				source_address = &storage.address_[reg];
				switch(mode) {
					case 0:		source = &storage.data_[reg];			break;
					case 1:		source = &storage.address_[reg];		break;
					default:	source = &storage.source_bus_data_[0];	break;
				}
			}

			void set_destination(ProcessorStorage &storage, int mode, int reg) {
				destination_address = &storage.address_[reg];
				switch(mode) {
					case 0:		destination = &storage.data_[reg];					break;
					case 1:		destination = &storage.address_[reg];				break;
					default:	destination = &storage.destination_bus_data_[0];	break;
				}
			}
		};

		// Storage for all the sequences of bus steps and micro-ops used throughout
		// the 68000.
		std::vector<BusStep> all_bus_steps_;
		std::vector<MicroOp> all_micro_ops_;

		// A lookup table from instructions to implementations.
		Program instructions[65536];

		// Special steps for exception handlers.
		BusStep *reset_bus_steps_;

		// Special micro-op sequences and storage for conditionals.
		BusStep *branch_taken_bus_steps_;
		BusStep *branch_byte_not_taken_bus_steps_;
		BusStep *branch_word_not_taken_bus_steps_;
		BusStep *bsr_bus_steps_;

		uint32_t dbcc_false_address_;
		BusStep *dbcc_condition_true_steps_;
		BusStep *dbcc_condition_false_no_branch_steps_;
		BusStep *dbcc_condition_false_branch_steps_;

		BusStep *movem_read_steps_;
		BusStep *movem_write_steps_;

		BusStep *trap_steps_;

		// Current bus step pointer, and outer program pointer.
		Program *active_program_ = nullptr;
		MicroOp *active_micro_op_ = nullptr;
		BusStep *active_step_ = nullptr;
		uint16_t decoded_instruction_ = 0;
		uint16_t next_word_ = 0;

		/// Copies address_[7] to the proper stack pointer based on current mode.
		void write_back_stack_pointer();

		/// Sets or clears the supervisor flag, ensuring the stack pointer is properly updated.
		void set_is_supervisor(bool);

		// Transient storage for MOVEM, TRAP and others.
		uint32_t precomputed_addresses_[65];
		RegisterPair16 throwaway_value_;
		uint32_t movem_final_address_;

		/*!
			Evaluates the conditional described by @c code and returns @c true or @c false to
			indicate the result of that evaluation.
		*/
		inline bool evaluate_condition(uint8_t code) {
			switch(code & 0xf) {
				default:
				case 0x00:	return true;							// true
				case 0x01:	return false;							// false
				case 0x02:	return zero_result_ && !carry_flag_;	// high
				case 0x03:	return !zero_result_ || carry_flag_;	// low or same
				case 0x04:	return !carry_flag_;					// carry clear
				case 0x05:	return carry_flag_;						// carry set
				case 0x06:	return zero_result_;					// not equal
				case 0x07:	return !zero_result_;					// equal
				case 0x08:	return !overflow_flag_;					// overflow clear
				case 0x09:	return overflow_flag_;					// overflow set
				case 0x0a:	return !negative_flag_;					// positive
				case 0x0b:	return negative_flag_;					// negative
				case 0x0c:	// greater than or equal
					return (negative_flag_ && overflow_flag_) || (!negative_flag_ && !overflow_flag_);
				case 0x0d:	// less than
					return (negative_flag_ && !overflow_flag_) || (!negative_flag_ && overflow_flag_);
				case 0x0e:	// greater than
					return zero_result_ && ((negative_flag_ && overflow_flag_) || (!negative_flag_ && !overflow_flag_));
				case 0x0f:	// less than or equal
					return !zero_result_ || (negative_flag_ && !overflow_flag_) || (!negative_flag_ && overflow_flag_);
			}
		}


	private:
		friend class ProcessorStorageConstructor;
};

#endif /* MC68000Storage_h */
