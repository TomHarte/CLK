//
//  OperationMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../Reflection/Dispatcher.hpp"
#include "BarrelShifter.hpp"

namespace InstructionSet::ARM {

enum class Model {
	ARMv2,

	/// Like an ARMv2 but all non-PC addressing is 64-bit. Primarily useful for a particular set of test
	/// cases that I want to apply retroactively; not a real iteration.
	ARMv2with32bitAddressing,
};

enum class Condition {
	EQ,	NE,	CS,	CC,
	MI,	PL,	VS,	VC,
	HI,	LS,	GE,	LT,
	GT,	LE,	AL,	NV,
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
	uint32_t operand2() const				{	return opcode_ & 0xf;					}
	/// The type of shift to apply to operand 2 if @c operand2_is_immediate() is @c false; meaningless otherwise.
	ShiftType shift_type() const			{	return ShiftType((opcode_ >> 5) & 3);	}
	/// @returns @c true if the amount to shift by should be taken from a register; @c false if it is an immediate value.
	bool shift_count_is_register() const	{	return opcode_ & (1 << 4);				}
	/// The shift amount register index if @c shift_count_is_register() is @c true; meaningless otherwise.
	uint32_t shift_register() const			{	return (opcode_ >> 8) & 0xf;			}
	/// The amount to shift by if @c shift_count_is_register() is @c false; meaningless otherwise.
	uint32_t shift_amount() const			{	return (opcode_ >> 7) & 0x1f;			}

protected:
	uint32_t opcode_;
};

//
// Branch (i.e. B and BL).
//
struct BranchFlags {
	constexpr BranchFlags(uint8_t flags) noexcept : flags_(flags) {}

	enum class Operation {
		B,		/// Add offset to PC; programmer allows for PC being two words ahead.
		BL,		/// Copy PC and PSR to R14, then branch. Copied PC points to next instruction.
	};

	/// @returns The operation to apply.
	constexpr Operation operation() const {
		return flag_bit<24>(flags_) ? Operation::BL : Operation::B;
	}

private:
	uint8_t flags_;
};

struct Branch {
	constexpr Branch(uint32_t opcode) noexcept : opcode_(opcode) {}

	/// The 26-bit offset to add to the PC.
	uint32_t offset() const				{	return (opcode_ & 0xff'ffff) << 2;	}

private:
	uint32_t opcode_;
};

//
// Data processing (i.e. AND to MVN).
//
enum class DataProcessingOperation {
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
};

constexpr bool is_logical(DataProcessingOperation operation) {
	switch(operation) {
		case DataProcessingOperation::AND:
		case DataProcessingOperation::EOR:
		case DataProcessingOperation::TST:
		case DataProcessingOperation::TEQ:
		case DataProcessingOperation::ORR:
		case DataProcessingOperation::MOV:
		case DataProcessingOperation::BIC:
		case DataProcessingOperation::MVN:
			return true;

		default: return false;
	}
}

constexpr bool is_comparison(DataProcessingOperation operation) {
	switch(operation) {
		case DataProcessingOperation::TST:
		case DataProcessingOperation::TEQ:
		case DataProcessingOperation::CMP:
		case DataProcessingOperation::CMN:
			return true;

		default: return false;
	}
}

struct DataProcessingFlags {
	constexpr DataProcessingFlags(uint8_t flags) noexcept : flags_(flags) {}

	/// @returns The operation to apply.
	constexpr DataProcessingOperation operation() const {
		return DataProcessingOperation((flags_ >> (21 - FlagsStartBit)) & 0xf);
	}

	/// @returns @c true if operand 2 is defined by the @c rotate() and @c immediate() fields;
	///		@c false if it is defined by the @c shift_*() and @c operand2() fields.
	constexpr bool operand2_is_immediate() const	{	return flag_bit<25>(flags_);	}

	/// @c true if the status register should be updated; @c false otherwise.
	constexpr bool set_condition_codes() const		{	return flag_bit<20>(flags_);	}

private:
	uint8_t flags_;
};

struct DataProcessing: public WithShiftControlBits {
	using WithShiftControlBits::WithShiftControlBits;

	/// The destination register index. i.e. Rd.
	uint32_t destination() const		{	return (opcode_ >> 12) & 0xf;	}

	/// The operand 1 register index. i.e. Rn.
	uint32_t operand1() const			{	return (opcode_ >> 16) & 0xf;	}

	//
	// Immediate values for operand 2.
	//

	/// An 8-bit value to rotate right @c rotate() places if @c operand2_is_immediate() is @c true; meaningless otherwise.
	uint32_t immediate() const			{	return opcode_ & 0xff;			}
	/// The number of bits to rotate @c immediate()  by to the right if @c operand2_is_immediate() is @c true; meaningless otherwise.
	uint32_t rotate() const				{	return (opcode_ >> 7) & 0x1e;	}
};

//
// MUL and MLA.
//
struct MultiplyFlags {
	constexpr MultiplyFlags(uint8_t flags) noexcept : flags_(flags) {}

	/// @c true if the status register should be updated; @c false otherwise.
	constexpr bool set_condition_codes() const	{	return flag_bit<20>(flags_);	}

	enum class Operation {
		MUL,	/// Rd = Rm * Rs
		MLA,	/// Rd = Rm * Rs + Rn
	};

	/// @returns The operation to apply.
	constexpr Operation operation() const {
		return flag_bit<21>(flags_) ? Operation::MLA : Operation::MUL;
	}

private:
	uint8_t flags_;
};

struct Multiply {
	constexpr Multiply(uint32_t opcode) noexcept : opcode_(opcode) {}

	/// The destination register index. i.e. 'Rd'.
	uint32_t destination() const	{	return (opcode_ >> 16) & 0xf;	}

	/// The accumulator register index for multiply-add. i.e. 'Rn'.
	uint32_t accumulator() const	{	return (opcode_ >> 12) & 0xf;	}

	/// The multiplicand register index. i.e. 'Rs'.
	uint32_t multiplicand() const	{	return (opcode_ >> 8) & 0xf;	}

	/// The multiplier register index. i.e. 'Rm'.
	uint32_t multiplier() const		{	return opcode_ & 0xf;			}

private:
	uint32_t opcode_;
};

//
// Single data transfer (LDR, STR).
//
struct SingleDataTransferFlags {
	constexpr SingleDataTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	enum class Operation {
		LDR,	/// Read single byte or word from [base + offset], possibly mutating the base.
		STR,	/// Write a single byte or word to [base + offset], possibly mutating the base.
	};

	constexpr Operation operation() const {
		return flag_bit<20>(flags_) ? Operation::LDR : Operation::STR;
	}

	constexpr bool offset_is_register() const	{	return flag_bit<25>(flags_);	}
	constexpr bool pre_index() const			{	return flag_bit<24>(flags_);	}
	constexpr bool add_offset() const			{	return flag_bit<23>(flags_);	}
	constexpr bool transfer_byte() const		{	return flag_bit<22>(flags_);	}
	constexpr bool write_back_address() const	{	return flag_bit<21>(flags_);	}

private:
	uint8_t flags_;
};

struct SingleDataTransfer: public WithShiftControlBits {
	using WithShiftControlBits::WithShiftControlBits;

	/// The destination register index. i.e. 'Rd' for LDR.
	uint32_t destination() const		{	return (opcode_ >> 12) & 0xf;	}

	/// The destination register index. i.e. 'Rd' for STR.
	uint32_t source() const				{	return (opcode_ >> 12) & 0xf;	}

	/// The base register index. i.e. 'Rn'.
	uint32_t base() const				{	return (opcode_ >> 16) & 0xf;	}

	/// The immediate offset, if @c offset_is_register() was @c false; meaningless otherwise.
	uint32_t immediate() const			{	return opcode_ & 0xfff;			}
};

//
// Block data transfer (LDR, STR).
//
struct BlockDataTransferFlags {
	constexpr BlockDataTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	enum class Operation {
		LDM,	/// Read 1–16 words from [base], possibly mutating it.
		STM,	/// Write 1-16 words to [base], possibly mutating it.
	};

	constexpr Operation operation() const {
		return flag_bit<20>(flags_) ? Operation::LDM : Operation::STM;
	}

	constexpr bool pre_index() const			{	return flag_bit<24>(flags_);	}
	constexpr bool add_offset() const			{	return flag_bit<23>(flags_);	}
	constexpr bool load_psr() const				{	return flag_bit<22>(flags_);	}
	constexpr bool write_back_address() const	{	return flag_bit<21>(flags_);	}

private:
	uint8_t flags_;
};

struct BlockDataTransfer: public WithShiftControlBits {
	using WithShiftControlBits::WithShiftControlBits;

	/// The base register index. i.e. 'Rn'.
	uint32_t base() const				{	return (opcode_ >> 16) & 0xf;			}

	/// A bitfield indicating which registers to load or store.
	uint16_t register_list() const		{	return static_cast<uint16_t>(opcode_);	}
	uint32_t popcount() const {
		const uint16_t list = register_list();

		// TODO: just use std::popcount when adopting C++20.
		uint32_t total = ((list & 0xaaaa) >> 1) + (list & 0x5555);
		total = ((total & 0xcccc) >> 2) + (total & 0x3333);
		total = ((total & 0xf0f0) >> 4) + (total & 0x0f0f);
		total = ((total & 0xff00) >> 8) + (total & 0x00ff);

		return total;
	}
};

//
// Coprocessor data operation.
//
struct CoprocessorDataOperationFlags {
	constexpr CoprocessorDataOperationFlags(uint8_t flags) noexcept : flags_(flags) {}

	constexpr int coprocessor_operation() const		{	return (flags_ >> (FlagsStartBit - 20)) & 0xf;	}

private:
	uint8_t flags_;
};

struct CoprocessorDataOperation {
	constexpr CoprocessorDataOperation(uint32_t opcode) noexcept : opcode_(opcode) {}

	uint32_t operand1() const		{ return (opcode_ >> 16) & 0xf;	}
	uint32_t operand2() const		{ return opcode_ & 0xf; 		}
	uint32_t destination() const	{ return (opcode_ >> 12) & 0xf;	}
	uint32_t coprocessor() const	{ return (opcode_ >> 8) & 0xf;	}
	uint32_t information() const	{ return (opcode_ >> 5) & 0x7;	}

private:
	uint32_t opcode_;
};

//
// Coprocessor register transfer.
//
struct CoprocessorRegisterTransferFlags {
	constexpr CoprocessorRegisterTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	enum class Operation {
		MRC,	///	Move from coprocessor register to ARM register.
		MCR,	/// Move from ARM register to coprocessor register.
	};

	constexpr Operation operation() const {
		return flag_bit<20>(flags_) ? Operation::MRC : Operation::MCR;
	}
	constexpr int coprocessor_operation() const		{	return (flags_ >> (FlagsStartBit - 20)) & 0x7;	}

private:
	uint8_t flags_;
};

struct CoprocessorRegisterTransfer {
	constexpr CoprocessorRegisterTransfer(uint32_t opcode) noexcept : opcode_(opcode) {}

	uint32_t operand1() const		{ return (opcode_ >> 16) & 0xf;	}
	uint32_t operand2() const		{ return opcode_ & 0xf; 		}
	uint32_t destination() const	{ return (opcode_ >> 12) & 0xf;	}
	uint32_t coprocessor() const	{ return (opcode_ >> 8) & 0xf;	}
	uint32_t information() const	{ return (opcode_ >> 5) & 0x7;	}

private:
	uint32_t opcode_;
};

//
// Coprocessor data transfer.
//
struct CoprocessorDataTransferFlags {
	constexpr CoprocessorDataTransferFlags(uint8_t flags) noexcept : flags_(flags) {}

	enum class Operation {
		LDC,	/// Coprocessor data transfer load.
		STC,	/// Coprocessor data transfer store.
	};

	constexpr Operation operation() const {
		return flag_bit<20>(flags_) ? Operation::LDC : Operation::STC;
	}
	constexpr bool pre_index() const			{	return flag_bit<24>(flags_);	}
	constexpr bool add_offset() const			{	return flag_bit<23>(flags_);	}
	constexpr bool transfer_length() const		{	return flag_bit<22>(flags_);	}
	constexpr bool write_back_address() const	{	return flag_bit<21>(flags_);	}

private:
	uint8_t flags_;
};

//
// Software interrupt.
//
struct SoftwareInterrupt {
	constexpr SoftwareInterrupt(uint32_t opcode) noexcept : opcode_(opcode) {}

	/// The 24-bit comment field, often decoded by the receiver of an SWI.
	uint32_t comment() const				{	return opcode_ & 0xff'ffff;	}

private:
	uint32_t opcode_;
};

struct CoprocessorDataTransfer {
	constexpr CoprocessorDataTransfer(uint32_t opcode) noexcept : opcode_(opcode) {}

	int base() const		{ return (opcode_ >> 16) & 0xf;	}

	int source() const		{ return (opcode_ >> 12) & 0xf; }
	int destination() const	{ return (opcode_ >> 12) & 0xf;	}

	int coprocessor() const	{ return (opcode_ >> 8) & 0xf;	}
	int offset() const		{ return opcode_ & 0xff;		}

private:
	uint32_t opcode_;
};

/// Operation mapper; use the free function @c dispatch as defined below.
template <Model>
struct OperationMapper {
	static Condition condition(uint32_t instruction) {
		return Condition(instruction >> 28);
	}

	template <int i, typename SchedulerT>
	static void dispatch(uint32_t instruction, SchedulerT &scheduler) {
		// Put the 8-bit segment of instruction back into its proper place;
		// this allows all the tests below to be written so as to coordinate
		// properly with the data sheet, and since it's all compile-time work
		// it doesn't cost anything.
		constexpr auto partial = uint32_t(i << 20);

		// Cf. the ARM2 datasheet, p.45. Tests below match its ordering
		// other than that 'undefined' is the fallthrough case. More specific
		// page references are provided were more detailed versions of the
		// decoding are depicted.

		// Multiply and multiply-accumulate (MUL, MLA); cf. p.23.
		//
		// This usurps a potential data processing decoding, so needs priority.
		if constexpr (((partial >> 22) & 0b111'111) == 0b000'000) {
			// This implementation provides only eight bits baked into the template parameters so
			// an additional dynamic test is required to check whether this is really, really MUL or MLA.
			if((instruction & 0b1111'0000) == 0b1001'0000) {
				scheduler.template perform<i>(Multiply(instruction));
				return;
			}
		}

		// Data processing; cf. p.17.
		if constexpr (((partial >> 26) & 0b11) == 0b00) {
			scheduler.template perform<i>(DataProcessing(instruction));
			return;
		}

		// Single data transfer (LDR, STR); cf. p.25.
		if constexpr (((partial >> 26) & 0b11) == 0b01) {
			scheduler.template perform<i>(SingleDataTransfer(instruction));
			return;
		}

		// Block data transfer (LDM, STM); cf. p.29.
		if constexpr (((partial >> 25) & 0b111) == 0b100) {
			scheduler.template perform<i>(BlockDataTransfer(instruction));
			return;
		}

		// Branch and branch with link (B, BL); cf. p.15.
		if constexpr (((partial >> 25) & 0b111) == 0b101) {
			scheduler.template perform<i>(Branch(instruction));
			return;
		}

		// Software interreupt; cf. p.35.
		if constexpr (((partial >> 24) & 0b1111) == 0b1111) {
			scheduler.software_interrupt(SoftwareInterrupt(instruction));
			return;
		}

		// Both:
		// Coprocessor data operation; cf. p. 37; and
		// Coprocessor register transfers; cf. p. 42.
		if constexpr (((partial >> 24) & 0b1111) == 0b1110) {
			if(instruction & (1 << 4)) {
				// Register transfer.
				scheduler.template perform<i>(CoprocessorRegisterTransfer(instruction));
			} else {
				// Data operation.
				scheduler.template perform<i>(CoprocessorDataOperation(instruction));
			}
			return;
		}

		// Coprocessor data transfers; cf. p.39.
		if constexpr (((partial >> 25) & 0b111) == 0b110) {
			scheduler.template perform<i>(CoprocessorDataTransfer(instruction));
			return;
		}

		// Fallback position.
		scheduler.unknown();
	}
};

/// A brief documentation of the interface expected by @c dispatch below; will be a concept if/when this project adopts C++20.
struct SampleScheduler {
	/// @returns @c true if the rest of the instruction should be decoded and supplied
	/// to the scheduler as defined below; @c false otherwise.
	bool should_schedule(Condition condition);

	// Template argument:
	//
	//		Flags, an opaque type which can be converted into a DataProcessingFlags, MultiplyFlags, etc,
	//		by simple construction, to provide all flags that can be baked into the template parameters.
	//
	// Function argument:
	//
	//		An operation-specific encapsulation of the operation code for decoding of fields that didn't
	//		fit into the template parameters.
	//
	// Either or both may be omitted if unnecessary.
	template <Flags> void perform(DataProcessing);
	template <Flags> void perform(Multiply);
	template <Flags> void perform(SingleDataTransfer);
	template <Flags> void perform(BlockDataTransfer);
	template <Flags> void perform(Branch);
	template <Flags> void perform(CoprocessorRegisterTransfer);
	template <Flags> void perform(CoprocessorDataOperation);
	template <Flags> void perform(CoprocessorDataTransfer);

	// Irregular operations.
	void software_interrupt(SoftwareInterrupt);
	void unknown();
};

/// Decodes @c instruction, making an appropriate call into @c scheduler.
///
/// In lieu of C++20, see the sample definition of SampleScheduler above for the expected interface.
template <Model model, typename SchedulerT> void dispatch(uint32_t instruction, SchedulerT &scheduler) {
	OperationMapper<model> mapper;

	// Test condition.
	const auto condition = mapper.condition(instruction);
	if(!scheduler.should_schedule(condition)) {
		return;
	}

	// Dispatch body.
	Reflection::dispatch(mapper, (instruction >> FlagsStartBit) & 0xff, instruction, scheduler);
}

}
