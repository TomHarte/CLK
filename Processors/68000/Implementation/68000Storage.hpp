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

		enum class ExecutionState {
			/// The normal mode, this means the 68000 is expending processing effort.
			Executing,

			/// The 68000 is in a holding loop, waiting for either DTack or to be notified of a bus error.
			WaitingForDTack,

			/// Occurs after executing a STOP instruction; the processor will idle waiting for an interrupt or reset.
			Stopped,

			/// Occurs at the end of the current bus cycle after detection of the HALT input, continuing until
			/// HALT is no longer signalled.
			Halted,

			/// Signals a transition from some other straight directly to cueing up an interrupt.
			WillBeginInterrupt,
		} execution_state_ = ExecutionState::Executing;
		Microcycle dtack_cycle_;
		Microcycle stop_cycle_;

		// Various status parts.
		int is_supervisor_;
		int interrupt_level_;
		uint_fast32_t zero_result_;		// The zero flag is set if this value is zero.
		uint_fast32_t carry_flag_;		// The carry flag is set if this value is non-zero.
		uint_fast32_t extend_flag_;		// The extend flag is set if this value is non-zero.
		uint_fast32_t overflow_flag_;	// The overflow flag is set if this value is non-zero.
		uint_fast32_t negative_flag_;	// The negative flag is set if this value is non-zero.
		uint_fast32_t trace_flag_;		// The trace flag is set if this value is non-zero.

		uint_fast32_t last_trace_flag_ = 0;

		// Bus inputs.
		int bus_interrupt_level_ = 0;
		bool dtack_ = false;
		bool is_peripheral_address_ = false;
		bool bus_error_ = false;
		bool bus_request_ = false;
		bool bus_acknowledge_ = false;
		bool halt_ = false;

		// Holds the interrupt level that should be serviced at the next instruction
		// dispatch, if any.
		int pending_interrupt_level_ = 0;
		// Holds the interrupt level that is currently being serviced.
		// TODO: surely this doesn't need to be distinct from the pending_interrupt_level_?
		int accepted_interrupt_level_ = 0;
		bool is_starting_interrupt_ = false;

		// Generic sources and targets for memory operations;
		// by convention: [0] = source, [1] = destination.
		RegisterPair32 effective_address_[2];
		RegisterPair32 source_bus_data_;
		RegisterPair32 destination_bus_data_;

		HalfCycles half_cycles_left_to_run_;
		HalfCycles e_clock_phase_;

		enum class Operation: uint8_t {
			None,
			ABCD,	SBCD,	NBCD,

			ADDb,	ADDw,	ADDl,
			ADDQb,	ADDQw,	ADDQl,
			ADDAw,	ADDAl,
			ADDQAw,	ADDQAl,
			ADDXb,	ADDXw,	ADDXl,

			SUBb,	SUBw,	SUBl,
			SUBQb,	SUBQw,	SUBQl,
			SUBAw,	SUBAl,
			SUBQAw,	SUBQAl,
			SUBXb,	SUBXw,	SUBXl,

			MOVEb,	MOVEw,	MOVEl,	MOVEq,
			MOVEAw,	MOVEAl,
			PEA,

			MOVEtoSR, MOVEfromSR,
			MOVEtoCCR,

			ORItoSR,	ORItoCCR,
			ANDItoSR,	ANDItoCCR,
			EORItoSR,	EORItoCCR,

			BTSTb,	BTSTl,
			BCLRl,	BCLRb,
			CMPb,	CMPw,	CMPl,
			CMPAw,
			TSTb,	TSTw,	TSTl,

			JMP,	RTS,
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

			MOVEPtoRl, MOVEPtoRw,
			MOVEPtoMl, MOVEPtoMw,

			ANDb,	ANDw,	ANDl,
			EORb,	EORw,	EORl,
			NOTb, 	NOTw, 	NOTl,
			ORb,	ORw,	ORl,

			MULU,	MULS,
			DIVU,	DIVS,

			RTE_RTR,

			TRAP,	TRAPV,
			CHK,

			EXG,	SWAP,

			BCHGl,	BCHGb,
			BSETl,	BSETb,

			TAS,

			EXTbtow,	EXTwtol,

			LINK,	UNLINK,

			STOP,
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

			forceinline bool operator ==(const BusStep &rhs) const {
				if(action != rhs.action) return false;
				return microcycle == rhs.microcycle;
			}

			forceinline bool is_terminal() const {
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
			enum class Action: uint8_t {
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

				/// From the next word in the prefetch queue assembles a sign-extended long word in either or
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

				// Performs the necessary status word substitution for the current interrupt level,
				// and does the first part of initialising the trap steps.
				PrepareINT,

				// Observes the bus_error_, valid_peripheral_address_ and/or the value currently in
				// source_bus_data_ to determine an interrupt vector, and fills in the final trap
				// steps detail appropriately.
				PrepareINTVector,
			};
			static constexpr int SourceMask = 1 << 7;
			static constexpr int DestinationMask = 1 << 6;
			uint8_t action = uint8_t(Action::None);

			static constexpr uint16_t NoBusProgram = std::numeric_limits<uint16_t>::max();
			uint16_t bus_program = NoBusProgram;

			MicroOp() {}
			MicroOp(uint8_t action) : action(action) {}
			MicroOp(uint8_t action, uint16_t bus_program) : action(action), bus_program(bus_program) {}

			MicroOp(Action action) : MicroOp(uint8_t(action)) {}
			MicroOp(Action action, uint16_t bus_program) : MicroOp(uint8_t(action), bus_program) {}

			forceinline bool is_terminal() const {
				return bus_program == std::numeric_limits<uint16_t>::max();
			}
		};

		/*!
			A program represents the implementation of a particular opcode, as a sequence
			of micro-ops and, separately, the operation to perform plus whatever other
			fields the operation requires.

			Some of the fields are slightly convoluted in how they identify the information
			they reference; this is done to keep this struct as small as possible due to
			concerns about cache size.

			On the 64-bit Intel processor this emulator was developed on, the struct below
			adds up to 8 bytes; four for the initial uint32_t and then one each for the
			remaining fields, with no additional padding being inserted by the compiler.
		*/
		struct Program {
			/// The offset into the all_micro_ops_ at which micro-ops for this instruction begin,
			/// or std::numeric_limits<uint32_t>::max() if this is an invalid Program.
			uint32_t micro_operations = std::numeric_limits<uint32_t>::max();
			/// The overarching operation applied by this program when the moment comes.
			Operation operation;
			/// The number of bytes after the beginning of an instance of ProcessorStorage that the RegisterPair32 containing
			/// a source value for this operation lies at.
			uint8_t source_offset = 0;
			/// The number of bytes after the beginning of an instance of ProcessorStorage that the RegisterPair32 containing
			/// a destination value for this operation lies at.
			uint8_t destination_offset = 0;
			/// A bitfield comprised of:
			///	b7 = set if this program requires supervisor mode;
			/// b0–b2 = the source address register (for pre-decrement and post-increment actions); and
			/// b4-b6 = destination address register.
			uint8_t source_dest = 0;

			void set_source_address([[maybe_unused]] ProcessorStorage &storage, int index) {
				source_dest = uint8_t((source_dest & 0x0f) | (index << 4));
			}

			void set_destination_address([[maybe_unused]] ProcessorStorage &storage, int index) {
				source_dest = uint8_t((source_dest & 0xf0) | index);
			}

			void set_requires_supervisor(bool requires_supervisor) {
				source_dest = (source_dest & 0x7f) | (requires_supervisor ? 0x80 : 0x00);
			}

			void set_source(ProcessorStorage &storage, RegisterPair32 *target) {
				source_offset = decltype(source_offset)(reinterpret_cast<uint8_t *>(target) - reinterpret_cast<uint8_t *>(&storage));
				assert(source_offset == (reinterpret_cast<uint8_t *>(target) - reinterpret_cast<uint8_t *>(&storage)));
			}

			void set_destination(ProcessorStorage &storage, RegisterPair32 *target) {
				destination_offset = decltype(destination_offset)(reinterpret_cast<uint8_t *>(target) - reinterpret_cast<uint8_t *>(&storage));
				assert(destination_offset == (reinterpret_cast<uint8_t *>(target) - reinterpret_cast<uint8_t *>(&storage)));
			}

			void set_source(ProcessorStorage &storage, int mode, int reg) {
				set_source_address(storage, reg);
				switch(mode) {
					case 0:		set_source(storage, &storage.data_[reg]);		break;
					case 1:		set_source(storage, &storage.address_[reg]);	break;
					default:	set_source(storage, &storage.source_bus_data_);	break;
				}
			}

			void set_destination(ProcessorStorage &storage, int mode, int reg) {
				set_destination_address(storage, reg);
				switch(mode) {
					case 0:		set_destination(storage, &storage.data_[reg]);				break;
					case 1:		set_destination(storage, &storage.address_[reg]);			break;
					default:	set_destination(storage, &storage.destination_bus_data_);	break;
				}
			}
		};

		// Storage for all the sequences of bus steps and micro-ops used throughout
		// the 68000.
		std::vector<BusStep> all_bus_steps_;
		std::vector<MicroOp> all_micro_ops_;

		// A lookup table from instructions to implementations.
		Program instructions[65536];

		// Special steps and programs for exception handlers.
		BusStep *reset_bus_steps_;
		MicroOp *long_exception_micro_ops_;		// i.e. those that leave 14 bytes on the stack — bus error and address error.
		MicroOp *short_exception_micro_ops_;	// i.e. those that leave 6 bytes on the stack — everything else (other than interrupts).
		MicroOp *interrupt_micro_ops_;

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

		// These two are dynamically modified depending on the particular
		// TRAP and bus error.
		BusStep *trap_steps_;
		BusStep *bus_error_steps_;

		// Current bus step pointer, and outer program pointer.
		const Program *active_program_ = nullptr;
		const MicroOp *active_micro_op_ = nullptr;
		const BusStep *active_step_ = nullptr;
		RegisterPair16 decoded_instruction_ = 0;
		uint16_t next_word_ = 0;

		/// Copies address_[7] to the proper stack pointer based on current mode.
		void write_back_stack_pointer();

		/// Sets or clears the supervisor flag, ensuring the stack pointer is properly updated.
		void set_is_supervisor(bool);

		// Transient storage for MOVEM, TRAP and others.
		RegisterPair16 throwaway_value_;
		uint32_t movem_final_address_;
		uint32_t precomputed_addresses_[65];	// This is a big chunk of rarely-used storage. It's placed last deliberately.

		/*!
			Evaluates the conditional described by @c code and returns @c true or @c false to
			indicate the result of that evaluation.
		*/
		forceinline bool evaluate_condition(uint8_t code) {
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

		/*!
			Fills in the appropriate addresses and values to complete the TRAP steps — those
			representing a short-form exception — and mutates the status register as if one
			were beginning.
		*/
		forceinline void populate_trap_steps(uint32_t vector, uint16_t status) {
			// Fill in the status word value.
			destination_bus_data_.full = status;

			// Switch to supervisor mode, disable the trace bit.
			set_is_supervisor(true);
			trace_flag_ = last_trace_flag_ = 0;

			// Pick a vector.
			effective_address_[0].full = vector << 2;

			// Schedule the proper stack activity.
			precomputed_addresses_[0] = address_[7].full - 2;	// PC.l
			precomputed_addresses_[1] = address_[7].full - 6;	// status word (in destination_bus_data_)
			precomputed_addresses_[2] = address_[7].full - 4;	// PC.h
			address_[7].full -= 6;

			// Set the default timing.
			trap_steps_->microcycle.length = HalfCycles(8);
		}

		forceinline void populate_bus_error_steps(uint32_t vector, uint16_t status, uint16_t bus_status, RegisterPair32 faulting_address) {
			// Fill in the status word value.
			destination_bus_data_.halves.low.full = status;
			destination_bus_data_.halves.high.full = bus_status;
			effective_address_[1] = faulting_address;

			// Switch to supervisor mode, disable the trace bit.
			set_is_supervisor(true);
			trace_flag_ = last_trace_flag_ = 0;

			// Pick a vector.
			effective_address_[0].full = vector << 2;

			// Schedule the proper stack activity.
			precomputed_addresses_[0] = address_[7].full - 2;		// PC.l
			precomputed_addresses_[1] = address_[7].full - 6;		// status word
			precomputed_addresses_[2] = address_[7].full - 4;		// PC.h
			precomputed_addresses_[3] = address_[7].full - 8;		// current instruction
			precomputed_addresses_[4] = address_[7].full - 10;		// fault address.l
			precomputed_addresses_[5] = address_[7].full - 14;		// bus cycle status word
			precomputed_addresses_[6] = address_[7].full - 12;		// fault address.h
			address_[7].full -= 14;
		}

		inline uint16_t get_status() const;
		inline void set_status(uint16_t);

	private:
		friend struct ProcessorStorageConstructor;
		friend class ProcessorStorageTests;
		friend struct State;
};

#endif /* MC68000Storage_h */
