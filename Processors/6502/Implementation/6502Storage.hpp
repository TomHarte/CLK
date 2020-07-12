//
//  6502Storage.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef MOS6502Storage_h
#define MOS6502Storage_h

/*!
	A repository for all the internal state of a CPU::MOS6502::Processor; extracted into a separate base
	class in order to remove it from visibility within the main 6502.hpp.
*/
class ProcessorStorage {
	protected:
		ProcessorStorage(Personality);

		/*!
			This emulation functions by decomposing instructions into micro programs, consisting of the micro operations
			defined by MicroOp. Each micro op takes at most one cycle. By convention, those called CycleX take a cycle
			to perform whereas those called OperationX occur for free (so, in effect, their cost is loaded onto the next cycle).

			This micro-instruction set was put together in a fairly ad hoc fashion, I'm afraid, so is unlikely to be optimal.
		*/
		enum MicroOp: uint8_t {
			CycleFetchOperation,		// fetches (PC) to operation_, storing PC to last_operation_pc_ before incrementing it
			CycleFetchOperand,			// 6502: fetches from (PC) to operand_; 65C02: as 6502 unless operation_ indicates a one-cycle NOP, in which case this is a no0op
			OperationDecodeOperation,	// schedules the microprogram associated with operation_
			OperationMoveToNextProgram,	// either schedules the next fetch-decode-execute or an interrupt response if a request has been pending for at least one cycle

			CycleIncPCPushPCH,			// increments the PC and pushes PC.h to the stack
			CyclePushPCL,				// pushes PC.l to the stack
			CyclePushPCH,				// pushes PC.h to the stack
			CyclePushA,					// pushes A to the stack
			CyclePushX,					// pushes X to the stack
			CyclePushY,					// pushes Y to the stack
			CyclePushOperand,			// pushes operand_ to the stack

			OperationSetIRQFlags,		// 6502: sets I; 65C02: sets I and resets D
			OperationSetNMIRSTFlags,	// 6502: no-op. 65C02: resets D

			OperationBRKPickVector,		// 65C02: sets next_address_ to the BRK vector location; 6502: as 65C02 if no NMI is pending; otherwise sets next_address_ to the NMI address and resets the internal NMI-pending flag
			OperationNMIPickVector,		// sets next_address_ to the NMI vector
			OperationRSTPickVector,		// sets next_address_ to the RST vector
			CycleReadVectorLow,			// reads PC.l from next_address_
			CycleReadVectorHigh,		// reads PC.h from (next_address_+1)

			CycleReadFromS,				// performs a read from the stack pointer, throwing the result away
			CycleReadFromPC,			// performs a read from the program counter, throwing the result away

			CyclePullPCL,				// pulls PC.l from the stack
			CyclePullPCH,				// pulls PC.h from the stack
			CyclePullA,					// pulls A from the stack
			CyclePullX,					// pulls X from the stack
			CyclePullY,					// pulls Y from the stack
			CyclePullOperand,			// pulls operand_ from the stack

			CycleNoWritePush,				// decrements S as though it were a push, but reads from the new stack address instead of writing
			CycleReadAndIncrementPC,		// reads from the PC, throwing away the result, and increments the PC
			CycleIncrementPCAndReadStack,	// increments the PC and reads from the stack pointer, throwing away the result
			CycleIncrementPCReadPCHLoadPCL,	// increments the PC, schedules a read of PC.h from the post-incremented PC, then copies operand_ to PC.l
			CycleReadPCHLoadPCL,			// schedules a read of PC.h from the post-incremented PC, then copies operand_ to PC.l
			CycleReadAddressHLoadAddressL,	// increments the PC; copies operand_ to address_.l; reads address_.h from the new PC

			CycleReadPCLFromAddress,		// reads PC.l from address_
			CycleReadPCHFromAddressLowInc,	// increments address_.l and reads PC.h from address_
			CycleReadPCHFromAddressFixed,	// if address_.l is 0, increments address_.h; and reads PC.h from address_
			CycleReadPCHFromAddressInc,		// increments address_ and reads PC.h from it

			CycleLoadAddressAbsolute,		// copies operand_ to address_.l, increments the PC, reads address_.h from PC, increments the PC again
			OperationLoadAddressZeroPage,	// copies operand_ to address_ and increments the PC
			CycleLoadAddessZeroX,			// copies (operand_+x)&0xff to address_, increments the PC, and reads from operand_, throwing away the result
			CycleLoadAddessZeroY,			// copies (operand_+y)&0xff to address_, increments the PC, and reads from operand_, throwing away the result

			CycleAddXToAddressLow,			// calculates address_ + x and stores it to next_address_; copies next_address_.l back to address_.l; 6502: if address_ now does not equal next_address_, schedules a throwaway read from address_; 65C02: schedules a throaway read from PC-1
			CycleAddYToAddressLow,			// calculates address_ + y and stores it to next_address_; copies next_address_.l back to address_.l; 6502: if address_ now does not equal next_address_, schedules a throwaway read from address_; 65C02: schedules a throaway read from PC-1
			CycleAddXToAddressLowRead,		// calculates address_ + x and stores it to next_address; copies next_address.l back to address_.l; 6502: schedules a throwaway read from address_; 65C02: schedules a throaway read from PC-1
			CycleAddYToAddressLowRead,		// calculates address_ + y and stores it to next_address; copies next_address.l back to address_.l; 6502: schedules a throwaway read from address_; 65C02: schedules a throaway read from PC-1
			OperationCorrectAddressHigh,	// copies next_address_ to address_

			OperationIncrementPC,			// increments the PC
			CycleFetchOperandFromAddress,	// fetches operand_ from address_
			CycleWriteOperandToAddress,		// writes operand_ to address_

			CycleIncrementPCFetchAddressLowFromOperand,	// increments the PC and loads address_.l from (operand_)
			CycleAddXToOperandFetchAddressLow,			// adds x [in]to operand_, producing an 8-bit result, and reads address_.l from (operand_)
			CycleIncrementOperandFetchAddressHigh,		// increments operand_, producing an 8-bit result, and reads address_.h from (operand_)
			OperationDecrementOperand,					// decrements operand_
			OperationIncrementOperand,					// increments operand_
			CycleFetchAddressLowFromOperand,			// reads address_.l from (operand_)

			OperationORA,	// ORs operand_ into a, setting the negative and zero flags
			OperationAND,	// ANDs operand_ into a, setting the negative and zero flags
			OperationEOR,	// EORs operand_ into a, setting the negative and zero flags

			OperationINS,	// increments operand_, then performs an SBC of operand_ from a
			OperationADC,	// performs an ADC of operand_ into a_; if this is a 65C02 and decimal mode is set, performs an extra read to operand_ from address_
			OperationSBC,	// performs an SBC of operand_ from a_; if this is a 65C02 and decimal mode is set, performs an extra read to operand_ from address_

			OperationCMP,	// CMPs a and operand_, setting negative, zero and carry flags
			OperationCPX,	// CMPs x and operand_, setting negative, zero and carry flags
			OperationCPY,	// CMPs y and operand_, setting negative, zero and carry flags
			OperationBIT,	// sets the zero, negative and overflow flags as per a BIT of operand_ against a
			OperationBITNoNV,	// sets the zero flag as per a BIT of operand_ against a

			OperationLDA,	// loads a with operand_, setting the negative and zero flags
			OperationLDX,	// loads x with operand_, setting the negative and zero flags
			OperationLDY,	// loads y with operand_, setting the negative and zero flags
			OperationLAX,	// loads a and x with operand_, setting the negative and zero flags
			OperationCopyOperandToA,		// sets a_ = operand_, not setting any flags

			OperationSTA,	// loads operand_ with a
			OperationSTX,	// loads operand_ with x
			OperationSTY,	// loads operand_ with y
			OperationSTZ,	// loads operand_ with 0
			OperationSAX,	// loads operand_ with a & x
			OperationSHA,	// loads operand_ with a & x & (address.h+1)
			OperationSHX,	// loads operand_ with x & (address.h+1)
			OperationSHY,	// loads operand_ with y & (address.h+1)
			OperationSHS,	// loads s with a & x, then loads operand_ with s & (address.h+1)

			OperationASL,	// shifts operand_ left, moving the top bit into carry and setting the negative and zero flags
			OperationASO,	// performs an ASL of operand and ORs it into a
			OperationROL,	// performs a ROL of operand_
			OperationRLA,	// performs a ROL of operand_ and ANDs it into a
			OperationLSR,	// shifts operand_ right, setting carry, negative and zero flags
			OperationLSE,	// performs an LSR and EORs the result into a
			OperationASR,	// ANDs operand_ into a, then performs an LSR
			OperationROR,	// performs a ROR of operand_, setting carry, negative and zero flags
			OperationRRA,	// performs a ROR of operand_ but sets only the carry flag

			OperationCLC,	// resets the carry flag
			OperationCLI,	// resets I
			OperationCLV,	// resets the overflow flag
			OperationCLD,	// resets the decimal flag
			OperationSEC,	// sets the carry flag
			OperationSEI,	// sets I
			OperationSED,	// sets the decimal flag

			OperationRMB,	// resets the bit in operand_ implied by operatiopn_
			OperationSMB,	// sets the bit in operand_ implied by operatiopn_
			OperationTRB,	// sets zero according to operand_ & a, then resets any bits in operand_ that are set in a
			OperationTSB,	// sets zero according to operand_ & a, then sets any bits in operand_ that are set in a

			OperationINC,	// increments operand_, setting the negative and zero flags
			OperationDEC,	// decrements operand_, setting the negative and zero flags
			OperationINX,	// increments x, setting the negative and zero flags
			OperationDEX,	// decrements x, setting the negative and zero flags
			OperationINY,	// increments y, setting the negative and zero flags
			OperationDEY,	// decrements y, setting the negative and zero flags
			OperationINA,	// increments a, setting the negative and zero flags
			OperationDEA,	// decrements a, setting the negative and zero flags

			OperationBPL,	// schedules the branch program if the negative flag is clear
			OperationBMI,	// schedules the branch program if the negative flag is set
			OperationBVC,	// schedules the branch program if the overflow flag is clear
			OperationBVS,	// schedules the branch program if the overflow flag is set
			OperationBCC,	// schedules the branch program if the carry flag is clear
			OperationBCS,	// schedules the branch program if the carry flag is set
			OperationBNE,	// schedules the branch program if the zero flag is clear
			OperationBEQ,	// schedules the branch program if the zero flag is set; 65C02: otherwise jumps straight into a fetch-decode-execute without considering whether to take an interrupt
			OperationBRA,	// schedules the branch program

			OperationBBRBBS,	// inspecting the operation_, if the appropriate bit of operand_ is set or clear schedules a program to read and act upon the second operand; otherwise schedule a program to read and discard it

			OperationTXA,	// copies x to a, setting the zero and negative flags
			OperationTYA,	// copies y to a, setting the zero and negative flags
			OperationTXS,	// copies x to s
			OperationTAY,	// copies a to y, setting the zero and negative flags
			OperationTAX,	// copies a to x, setting the zero and negative flags
			OperationTSX,	// copies s to x, setting the zero and negative flags

			/* The following are amongst the 6502's undocumented (/unintended) operations */
			OperationARR,	// performs a mixture of ANDing operand_ into a, and shifting the result right
			OperationSBX,	// performs a mixture of an SBC of x&a and operand_, mutating x
			OperationLXA,	// loads a and x with (a | 0xee) & operand, setting the negative and zero flags
			OperationANE,	// loads a_ with (a | 0xee) & operand & x, setting the negative and zero flags
			OperationANC,	// ANDs operand_ into a, setting the negative and zero flags, and loading carry as if the result were shifted right
			OperationLAS,	// loads a, x and s with s & operand, setting the negative and zero flags

			CycleFetchFromHalfUpdatedPC,		// performs a throwaway read from (PC + (signed)operand).l combined with PC.h
			CycleAddSignedOperandToPC,			// sets next_address to PC + (signed)operand. If the high byte of next_address differs from the PC, schedules a throwaway read from the half-updated PC. 65C02 specific: if the top two bytes are the same, proceeds directly to fetch-decode-execute, ignoring any pending interrupts.
			OperationAddSignedOperandToPC16,	// adds (signed)operand into the PC

			OperationSetFlagsFromOperand,			// sets all flags based on operand_
			OperationSetOperandFromFlagsWithBRKSet,	// sets operand_ to the value of all flags, with the break flag set
			OperationSetOperandFromFlags,			// sets operand_ to the value of all flags

			OperationSetFlagsFromA,		// sets the zero and negative flags based on the value of a
			OperationSetFlagsFromX,		// sets the zero and negative flags based on the value of x
			OperationSetFlagsFromY,		// sets the zero and negative flags based on the value of y

			OperationScheduleJam,		// schedules the program for operation F2
			OperationScheduleWait,		// puts the processor into WAI mode (i.e. it'll do nothing until an interrupt is received)
			OperationScheduleStop,		// puts the processor into STP mode (i.e. it'll do nothing until a reset is received)
		};

		using InstructionList = MicroOp[12];
		/// Defines the locations in operations_ of various named microprograms; the first 256 entries
		/// in operations_ are mapped directly from instruction codes and therefore not named.
		enum class OperationsSlot {
			/// Fetches the next operation, and its operand, then schedules the corresponding set of operations_.
			/// [Caveat: the 65C02 adds single-cycle NOPs; this microprogram won't fetch an operand for those].
			FetchDecodeExecute = 256,

			/// Performs the 6502's reset sequence.
			Reset,
			/// Performs the 6502's IRQ sequence.
			IRQ,
			/// Performs the 6502's NMI sequence.
			NMI,

			/// Performs a branch, e.g. the entry for BCC will evaluate whether carry is clear and, if so, will jump
			/// to this instruction list.
			DoBRA,

			/// On a 65c02,
			DoBBRBBS,
			DoNotBBRBBS,

			Max
		};
		InstructionList operations_[size_t(OperationsSlot::Max)];

		const MicroOp *scheduled_program_counter_ = nullptr;

		/*
			Storage for the 6502 registers; F is stored as individual flags.
		*/
		RegisterPair16 pc_, last_operation_pc_;
		uint8_t a_, x_, y_, s_ = 0;
		uint8_t carry_flag_, negative_result_, zero_result_, decimal_flag_, overflow_flag_, inverse_interrupt_flag_ = 0;

		/*
			Temporary state for the micro programs.
		*/
		uint8_t operation_, operand_;
		RegisterPair16 address_, next_address_;

		/*
			Temporary storage allowing a common dispatch point for calling perform_bus_operation;
			possibly deferring is no longer of value.
		*/
		BusOperation next_bus_operation_ = BusOperation::None;
		uint16_t bus_address_;
		uint8_t *bus_value_;

		/*!
			Gets the flags register.

			@see set_flags

			@returns The current value of the flags register.
		*/
		inline uint8_t get_flags() const;

		/*!
			Sets the flags register.

			@see set_flags

			@param flags The new value of the flags register.
		*/
		inline void set_flags(uint8_t flags);

		bool is_jammed_ = false;
		Cycles cycles_left_to_run_;

		enum InterruptRequestFlags: uint8_t {
			Reset		= 0x80,
			IRQ			= Flag::Interrupt,
			NMI			= 0x20,

			PowerOn		= 0x10,
		};
		uint8_t interrupt_requests_ = InterruptRequestFlags::PowerOn;

		bool ready_is_active_ = false;
		bool ready_line_is_enabled_ = false;
		bool stop_is_active_ = false;
		bool wait_is_active_ = false;

		uint8_t irq_line_ = 0, irq_request_history_ = 0;
		bool nmi_line_is_enabled_ = false, set_overflow_line_is_enabled_ = false;

		// Allow state objects to capture and apply state.
		friend struct State;
};

#endif /* _502Storage_h */
