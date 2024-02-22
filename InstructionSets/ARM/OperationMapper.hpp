//
//  OperationMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Reflection/Dispatcher.hpp"

namespace InstructionSet::ARM {

enum class Model {
	ARM2,
};

enum class Operation {
	AND,	/// Rd = Op1 AND Op2.
	EOR,	/// Rd = Op1 EOR Op2.
	SUB,	/// Rd = Op1 - Op2.
	RSB,	/// Rd = Op2 - Op1.
	ADD,	/// Rd = Op1 + Op2.
	ADC,	/// Rd = Op1 + Ord2 + C.
	SBC,	/// Rd = Op1 - Op2 + C.
	RSC,	/// Rd = Op2 - Op1 + C.
	TST,	/// Set condition codes on Op1 AND Op2.
	TEQ,	/// Set condition codes on Op1 EOR Op2.
	CMP,	/// Set condition codes on Op1 - Op2.
	CMN,	/// Set condition codes on Op1 + Op2.
	ORR,	/// Rd = Op1 OR Op2.
	MOV,	/// Rd = Op2
	BIC,	/// Rd = Op1 AND NOT Op2.
	MVN,	/// Rd = NOT Op2.

	MUL,	/// Rd = Rm * Rs
	MLA,	/// Rd = Rm * Rs + Rn
	B,		/// Add offset to PC; programmer allows for PC being two words ahead.
	BL,		/// Copy PC and PSR to R14, then branch. Copied PC points to next instruction.

	LDR,	/// Read single byte or word from [base + offset], possibly mutating the base.
	STR,	/// Write a single byte or word to [base + offset], possibly mutating the base.
	LDM,	/// Read 1–16 words from [base], possibly mutating it.
	STM,	/// Write 1-16 words to [base], possibly mutating it.
	SWI,	/// Perform a software interrupt.

	CDP,	/// Coprocessor data operation.
	MRC,	///	Move from coprocessor register to ARM register.
	MCR,	/// Move from ARM register to coprocessor register.
	LDC,	/// Coprocessor data transfer load.
	STC,	/// Coprocessor data transfer store.

	Undefined,
};

enum class Condition {
	EQ,	NE,	CS,	CC,
	MI,	PL,	VS,	VC,
	HI,	LS,	GE,	LT,
	GT,	LE,	AL,	NV,
};

enum class ShiftType {
	LogicalLeft = 0b00,
	LogicalRight = 0b01,
	ArithmeticRight = 0b10,
	RotateRight = 0b11,
};

//
// Implementation details.
//
static constexpr int FlagsStartBit = 20;
using Flags = uint8_t;

template <int position>
constexpr bool flag_bit(uint8_t flags) {
	static_assert(position >= 20 && position < 28);
	return flags & (1 << (position - FlagsStartBit));
}

//
// Methods common to data processing and data transfer.
//
struct WithShiftControlBits {
	constexpr WithShiftControlBits(uint32_t opcode) noexcept : opcode_(opcode) {}

	/// The operand 2 register index if @c operand2_is_immediate() is @c false; meaningless otherwise.
	int operand2() const					{	return opcode_ & 0xf;			}
	/// The type of shift to apply to operand 2 if @c operand2_is_immediate() is @c false; meaningless otherwise.
	ShiftType shift_type() const			{	return ShiftType((opcode_ >> 5) & 3);	}
	/// @returns @c true if the amount to shift by should be taken from a register; @c false if it is an immediate value.
	bool shift_count_is_register() const	{	return opcode_ & (1 << 4);				}
	/// The shift amount register index if @c shift_count_is_register() is @c true; meaningless otherwise.
	int shift_register() const				{	return (opcode_ >> 8) & 0xf;			}
	/// The amount to shift by if @c shift_count_is_register() is @c false; meaningless otherwise.
	int shift_amount() const				{	return (opcode_ >> 7) & 0x1f;			}

protected:
	uint32_t opcode_;
};

//
// Branch (i.e. B and BL).
//
struct Branch {
	constexpr Branch(uint32_t opcode) noexcept : opcode_(opcode) {}

	/// The 26-bit offset to add to the PC.
	int offset() const				{	return (opcode_ & 0xff'ffff) << 2;	}

private:
	uint32_t opcode_;
};

//
// Data processing (i.e. AND to MVN).
//
struct DataProcessingFlags {
	constexpr DataProcessingFlags(uint8_t flags) noexcept : flags_(flags) {}

	/// @returns @c true if operand 2 is defined by the @c rotate() and @c immediate() fields;
	///		@c false if it is defined by the @c shift_*() and @c operand2() fields.
	constexpr bool operand2_is_immediate()	{	return flag_bit<25>(flags_);	}

	/// @c true if the status register should be updated; @c false otherwise.
	constexpr bool set_condition_codes()	{	return flag_bit<20>(flags_);	}

private:
	uint8_t flags_;
};

struct DataProcessing: public WithShiftControlBits {
	using WithShiftControlBits::WithShiftControlBits;

	/// The destination register index. i.e. Rd.
	int destination() const				{	return (opcode_ >> 12) & 0xf;	}

	/// The operand 1 register index. i.e. Rn.
	int operand1() const				{	return (opcode_ >> 16) & 0xf;	}

	//
	// Immediate values for operand 2.
	//

	/// An 8-bit value to rotate right @c rotate() places if @c operand2_is_immediate() is @c true; meaningless otherwise.
	int immediate() const				{	return opcode_ & 0xff;			}
	/// The number of bits to rotate @c immediate()  by to the right if @c operand2_is_immediate() is @c true; meaningless otherwise.
	int rotate() const					{	return (opcode_ >> 7) & 0x1e;	}
};

//
// MUL and MLA.
//
struct MultiplyFlags {
	constexpr MultiplyFlags(uint8_t flags) noexcept : flags_(flags) {}

	/// @c true if the status register should be updated; @c false otherwise.
	constexpr bool set_condition_codes()	{	return flag_bit<20>(flags_);	}

private:
	uint8_t flags_;
};

struct Multiply {
	constexpr Multiply(uint32_t opcode) noexcept : opcode_(opcode) {}

	/// The destination register index. i.e. 'Rd'.
	int destination() const				{	return (opcode_ >> 16) & 0xf;	}

	/// The accumulator register index for multiply-add. i.e. 'Rn'.
	int accumulator() const				{	return (opcode_ >> 12) & 0xf;	}

	/// The multiplicand register index. i.e. 'Rs'.
	int multiplicand() const			{	return (opcode_ >> 8) & 0xf;	}

	/// The multiplier register index. i.e. 'Rm'.
	int multiplier() const				{	return opcode_ & 0xf;			}

private:
	uint32_t opcode_;
};

//
// Single data transfer (LDR, STR).
//
struct SingleDataTransferFlags {
	constexpr SingleDataTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	constexpr bool offset_is_immediate()	{	return flag_bit<25>(flags_);	}
	constexpr bool pre_index()				{	return flag_bit<24>(flags_);	}
	constexpr bool add_offset()				{	return flag_bit<23>(flags_);	}
	constexpr bool transfer_byte()			{	return flag_bit<22>(flags_);	}
	constexpr bool write_back_address()		{	return flag_bit<21>(flags_);	}

private:
	uint8_t flags_;
};

struct SingleDataTransfer: public WithShiftControlBits {
	using WithShiftControlBits::WithShiftControlBits;

	/// The destination register index. i.e. 'Rd' for LDR.
	int destination() const				{	return (opcode_ >> 12) & 0xf;	}

	/// The destination register index. i.e. 'Rd' for STR.
	int source() const					{	return (opcode_ >> 12) & 0xf;	}

	/// The base register index. i.e. 'Rn'.
	int base() const					{	return (opcode_ >> 16) & 0xf;	}

	/// The immediate offset, if @c offset_is_immediate() was @c true; meaningless otherwise.
	int immediate() const				{	return opcode_ & 0xfff;			}
};

//
// Block data transfer (LDR, STR).
//
struct BlockDataTransferFlags {
	constexpr BlockDataTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	constexpr bool pre_index()				{	return flag_bit<24>(flags_);	}
	constexpr bool add_offset()				{	return flag_bit<23>(flags_);	}
	constexpr bool load_psr()				{	return flag_bit<22>(flags_);	}
	constexpr bool write_back_address()		{	return flag_bit<21>(flags_);	}

private:
	uint8_t flags_;
};

struct BlockDataTransfer: public WithShiftControlBits {
	using WithShiftControlBits::WithShiftControlBits;

	/// The base register index. i.e. 'Rn'.
	int base() const					{	return (opcode_ >> 16) & 0xf;	}

	/// A bitfield indicating which registers to load or store.
	int register_list() const			{	return opcode_ & 0xffff;		}
};

//
// Coprocessor data operation.
//
struct CoprocessorDataOperationFlags {
	constexpr CoprocessorDataOperationFlags(uint8_t flags) noexcept : flags_(flags) {}

	constexpr int operation() const		{	return (flags_ >> (FlagsStartBit - 20)) & 0xf;	}

private:
	uint8_t flags_;
};

struct CoprocessorDataOperation {
	constexpr CoprocessorDataOperation(uint32_t opcode) noexcept : opcode_(opcode) {}

	int operand1()		{ return (opcode_ >> 16) & 0xf;	}
	int operand2()		{ return opcode_ & 0xf; 		}
	int destination()	{ return (opcode_ >> 12) & 0xf;	}
	int coprocessor()	{ return (opcode_ >> 8) & 0xf;	}
	int information()	{ return (opcode_ >> 5) & 0x7;	}

private:
	uint32_t opcode_;
};

//
// Coprocessor register transfer.
//
struct CoprocessorRegisterTransferFlags {
	constexpr CoprocessorRegisterTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	constexpr int operation() const		{	return (flags_ >> (FlagsStartBit - 20)) & 0x7;	}

private:
	uint8_t flags_;
};

struct CoprocessorRegisterTransfer {
	constexpr CoprocessorRegisterTransfer(uint32_t opcode) noexcept : opcode_(opcode) {}

	int operand1()		{ return (opcode_ >> 16) & 0xf;	}
	int operand2()		{ return opcode_ & 0xf; 		}
	int destination()	{ return (opcode_ >> 12) & 0xf;	}
	int coprocessor()	{ return (opcode_ >> 8) & 0xf;	}
	int information()	{ return (opcode_ >> 5) & 0x7;	}

private:
	uint32_t opcode_;
};

//
// Coprocessor data transfer.
//
struct CoprocessorDataTransferFlags {
	constexpr CoprocessorDataTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	constexpr bool pre_index()				{	return flag_bit<24>(flags_);	}
	constexpr bool add_offset()				{	return flag_bit<23>(flags_);	}
	constexpr bool transfer_length()		{	return flag_bit<22>(flags_);	}
	constexpr bool write_back_address()		{	return flag_bit<21>(flags_);	}

private:
	uint8_t flags_;
};

struct CoprocessorDataTransfer {
	constexpr CoprocessorDataTransfer(uint32_t opcode) noexcept : opcode_(opcode) {}

	int base()			{ return (opcode_ >> 16) & 0xf;	}

	int source()		{ return (opcode_ >> 12) & 0xf; }
	int destination()	{ return (opcode_ >> 12) & 0xf;	}

	int coprocessor()	{ return (opcode_ >> 8) & 0xf;	}
	int offset()		{ return opcode_ & 0xff;		}

private:
	uint32_t opcode_;
};

/// Operation mapper; use the free function @c dispatch as defined below.
struct OperationMapper {
	template <int i, typename SchedulerT> void dispatch(uint32_t instruction, SchedulerT &scheduler) {
		constexpr auto partial = uint32_t(i << 20);
		const auto condition = 	Condition(instruction >> 28);

		// Cf. the ARM2 datasheet, p.45. Tests below match its ordering
		// other than that 'undefined' is the fallthrough case. More specific
		// page references are provided were more detailed versions of the
		// decoding are depicted.

		// Data processing; cf. p.17.
		if constexpr (((partial >> 26) & 0b11) == 0b00) {
			constexpr auto operation = Operation(int(Operation::AND) + ((partial >> 21) & 0xf));
			scheduler.template perform<operation, i>(
				condition,
				DataProcessing(instruction)
			);
			return;
		}

		// Multiply and multiply-accumulate (MUL, MLA); cf. p.23.
		if constexpr (((partial >> 22) & 0b111'111) == 0b000'000) {
			// This implementation provides only eight bits baked into the template parameters so
			// an additional dynamic test is required to check whether this is really, really MUL or MLA.
			if(((instruction >> 4) & 0b1111) == 0b1001) {
				constexpr bool is_mla = partial & (1 << 21);
				scheduler.template perform<is_mla ? Operation::MLA : Operation::MUL, i>(
					condition,
					Multiply(instruction)
				);
				return;
			}
		}

		// Single data transfer (LDR, STR); cf. p.25.
		if constexpr (((partial >> 26) & 0b11) == 0b01) {
			constexpr bool is_ldr = partial & (1 << 20);
			scheduler.template perform<is_ldr ? Operation::LDR : Operation::STR, i>(
				condition,
				SingleDataTransfer(instruction)
			);
			return;
		}

		// Block data transfer (LDM, STM); cf. p.29.
		if constexpr (((partial >> 25) & 0b111) == 0b100) {
			constexpr bool is_ldm = partial & (1 << 20);
			scheduler.template perform<is_ldm ? Operation::LDM : Operation::STM, i>(
				condition,
				BlockDataTransfer(instruction)
			);
			return;
		}

		// Branch and branch with link (B, BL); cf. p.15.
		if constexpr (((partial >> 25) & 0b111) == 0b101) {
			constexpr bool is_bl = partial & (1 << 24);
			scheduler.template perform<is_bl ? Operation::BL : Operation::B>(
				condition,
				Branch(instruction)
			);
			return;
		}

		// Software interreupt; cf. p.35.
		if constexpr (((partial >> 24) & 0b1111) == 0b1111) {
			scheduler.software_interrupt(condition);
			return;
		}

		// Both:
		// Coprocessor data operation; cf. p. 37; and
		// Coprocessor register transfers; cf. p. 42.
		if constexpr (((partial >> 24) & 0b1111) == 0b1110) {
			if(instruction & (1 << 4)) {
				// Register transfer.
				const auto parameters = CoprocessorRegisterTransfer(instruction);
				constexpr bool is_mrc = partial & (1 << 20);
				scheduler.template perform<is_mrc ? Operation::MRC : Operation::MCR, i>(
					condition,
					parameters
				);
			} else {
				// Data operation.
				const auto parameters = CoprocessorDataOperation(instruction);
				scheduler.template perform<i>(
					condition,
					parameters
				);
			}
			return;
		}

		// Coprocessor data transfers; cf. p.39.
		if constexpr (((partial >> 25) & 0b111) == 0b110) {
			constexpr bool is_ldc = partial & (1 << 20);
			scheduler.template perform<is_ldc ? Operation::LDC : Operation::STC, i>(
				condition,
				CoprocessorDataTransfer(instruction)
			);
			return;
		}

		// Fallback position.
		scheduler.unknown(instruction);
	}
};

/// A brief documentation of the interface expected by @c dispatch below; will be a concept if/when this project adopts C++20.
struct SampleScheduler {
	// General template arguments:
	//
	//	(1) Operation, telling the function which operation to perform. Will always be from the subset
	//		implied by the operation category; and
	//	(2)	Flags, an opaque type which can be converted into a DataProcessingFlags, MultiplyFlags, etc,
	//		by simply construction, to provide all flags that can be baked into the template parameters.
	//
	// Arguments are ommitted if not relevant.
	//
	// Function arguments:
	//
	// 	(1)	Condition, indicating the condition code associated with this operation; and
	//	(2)	An operation-specific encapsulation of the operation code for decoding of fields that didn't
	//		fit into the template parameters.
	template <Operation, Flags> void perform(Condition, DataProcessing);
	template <Operation, Flags> void perform(Condition, Multiply);
	template <Operation, Flags> void perform(Condition, SingleDataTransfer);
	template <Operation, Flags> void perform(Condition, BlockDataTransfer);
	template <Operation> void perform(Condition, Branch);
	template <Operation, Flags> void perform(Condition, CoprocessorRegisterTransfer);
	template <Flags> void perform(Condition, CoprocessorDataOperation);
	template<Operation, Flags> void perform(Condition, CoprocessorDataTransfer);

	// Irregular operations.
	void software_interrupt(Condition);
	void unknown(uint32_t opcode);
};

/// Decodes @c instruction, making an appropriate call into @c scheduler.
///
/// In lieue of C++20, see the sample definition of SampleScheduler above for the expected interface.
template <typename SchedulerT> void dispatch(uint32_t instruction, SchedulerT &scheduler) {
	OperationMapper mapper;
	Reflection::dispatch(mapper, (instruction >> FlagsStartBit) & 0xff, instruction, scheduler);
}

}
