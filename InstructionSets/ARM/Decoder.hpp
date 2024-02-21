//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Model.hpp"
#include "Operation.hpp"

#include "../../Reflection/Dispatcher.hpp"

#include <array>

namespace InstructionSet::ARM {

enum class ShiftType {
	LogicalLeft = 0b00,
	LogicalRight = 0b01,
	ArithmeticRight = 0b10,
	RotateRight = 0b11,
};


static constexpr int FlagsStartBit = 20;

template <int position>
constexpr bool flag_bit(uint8_t flags) {
	static_assert(position >= 20 && position < 28);
	return flags & (1 << (position - FlagsStartBit));
}

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
// Data processing (i.e. AND to MVN).
//
struct DataProcessingFlags {
	constexpr DataProcessingFlags(uint8_t flags) noexcept : flags_(flags) {}

	/// @returns @c true if operand 2 is defined by the @c rotate() and @c immediate() fields;
	///		@c false if it is defined by the @c shift_*() and @c operand2() fields.
	constexpr bool operand2_is_immediate()	{	return flag_bit<25>(flags_);	}

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


struct OperationMapper {
	template <int i, typename SchedulerT> void dispatch(uint32_t instruction, SchedulerT &scheduler) {
		constexpr auto partial = static_cast<uint32_t>(i << 20);

		// Cf. the ARM2 datasheet, p.45. Tests below match its ordering
		// other than that 'undefined' is the fallthrough case. More specific
		// page references are provided were more detailed versions of the
		// decoding are depicted.

		// Data processing; cf. p.17.
		if constexpr (((partial >> 26) & 0b11) == 0b00) {
			constexpr auto operation = Operation(int(Operation::AND) + ((partial >> 21) & 0xf));
			constexpr auto flags = DataProcessingFlags(i);
			scheduler.template data_processing<operation, flags>(
				DataProcessing(instruction)
			);
		}

		// Multiply and multiply-accumulate (MUL, MLA); cf. p.23.
		if constexpr (((partial >> 22) & 0b111'111) == 0b000'000) {
			// This implementation provides only eight bits baked into the template parameters so
			// an additional dynamic test is required to check whether this is really, really MUL or MLA.
			if(((instruction >> 4) & 0b1111) != 0b1001) {
				scheduler.unknown(instruction);
			} else {
				constexpr bool is_mla = partial & (1 << 21);
				constexpr auto flags = MultiplyFlags(i);
				scheduler.template multiply<is_mla ? Operation::MLA : Operation::MUL, flags>(
					Multiply(instruction)
				);
			}
		}

		// Single data transfer (LDR, STR); cf. p.25.
		if constexpr (((partial >> 26) & 0b11) == 0b01) {
			constexpr bool is_ldr = partial & (1 << 20);
			constexpr auto flags = SingleDataTransferFlags(i);
			scheduler.template single_data_transfer<is_ldr ? Operation::LDR : Operation::STR, flags>(
				SingleDataTransfer(instruction)
			);
		}
	}
};

/// Decodes @c instruction, making an appropriate call into @c scheduler.
template <typename SchedulerT> void dispatch(uint32_t instruction, SchedulerT &scheduler) {
	OperationMapper mapper;
	Reflection::dispatch(mapper, (instruction >> FlagsStartBit) & 0xff, instruction, scheduler);
}

/*



		// Block data transfer (LDM, STM); cf. p.29.
		if(((opcode >> 25) & 0b111) == 0b100) {
			result[c] =
				((opcode >> 20) & 1) ? Operation::LDM : Operation::STM;
			continue;
		}

		// Branch and branch with link (B, BL); cf. p.15.
		if(((opcode >> 25) & 0b111) == 0b101) {
			result[c] =
				((opcode >> 24) & 1) ? Operation::BL : Operation::B;
			continue;
		}

		if(((opcode >> 25) & 0b111) == 0b110) {
			result[c] = Operation::CoprocessorDataTransfer;
			continue;
		}

		if(((opcode >> 24) & 0b1111) == 0b1110) {
			result[c] = Operation::CoprocessorDataOperationOrRegisterTransfer;
			continue;
		}

		if(((opcode >> 24) & 0b1111) == 0b1111) {
			result[c] = Operation::SoftwareInterrupt;
			continue;
		}

		result[c] = Operation::Undefined;

*/

}
