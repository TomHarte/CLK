//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "Decoder.hpp"
#include "Model.hpp"
#include "Operation.hpp"

namespace InstructionSet::ARM {

enum class ShiftType {
	LogicalLeft = 0b00,
	LogicalRight = 0b01,
	ArithmeticRight = 0b10,
	RotateRight = 0b11,
};

template <Model model>
class Instruction {
	public:
		constexpr Instruction(uint32_t opcode) noexcept : opcode_(opcode) {}

		constexpr Condition condition() const {	return Condition(opcode_ >> 28);	}
		constexpr Operation operation() const {
			return InstructionSet::ARM::operation<model>(opcode_);
		}

		//
		// B and BL.
		//
		struct Branch {
			constexpr Branch(uint32_t opcode) noexcept : opcode_(opcode) {}

			/// Provides a 26-bit offset to add to the program counter for B and BL.
			uint32_t offset() const 			{ return (opcode_ & 0xff'ffff) << 2; }

		private:
			uint32_t opcode_;
		};
		Branch branch() const { return Branch(opcode_); }

		//
		// Data processing (i.e. AND to MVN).
		//
		struct DataProcessing {
			constexpr DataProcessing(uint32_t opcode) noexcept : opcode_(opcode) {}

			/// The destination register index.
			int destination() const				{	return (opcode_ >> 12) & 0xf;	}

			/// The operand 1 register index.
			int operand1() const				{	return (opcode_ >> 16) & 0xf;	}

			/// @returns @c true if operand 2 is defined by the @c rotate() and @c immediate() fields;
			///		@c false if it is defined by the @c shift_*() and @c operand2() fields.
			bool operand2_is_immediate() const	{	return opcode_ & (1 << 25);		}

			//
			// Register values for operand 2.
			//

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

			//
			// Immediate values for operand 2.
			//

			/// An 8-bit value to rotate right @c rotate() places if @c operand2_is_immediate() is @c true; meaningless otherwise.
			int immediate() const				{	return opcode_ & 0xff;			}
			/// The number of bits to rotate @c immediate()  by if @c operand2_is_immediate() is @c true; meaningless otherwise.
			int rotate() const					{	return (opcode_ >> 7) & 0x1e;	}

		private:
			uint32_t opcode_;
		};
		DataProcessing data_processing() const { return DataProcessing(opcode_); }

		//
		// MUL and MLA.
		//
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
		Multiply multiply() const { return Multiply(opcode_); }

		//
		// LDR and STR.
		//
		struct SingleDataTransfer {
			constexpr SingleDataTransfer(uint32_t opcode) noexcept : opcode_(opcode) {}

			/// The destination register index. i.e. 'Rd' for LDR.
			int destination() const				{	return (opcode_ >> 12) & 0xf;	}

			/// The destination register index. i.e. 'Rd' for STR.
			int source() const					{	return (opcode_ >> 12) & 0xf;	}

			/// The base register index. i.e. 'Rn'.
			int base() const					{	return (opcode_ >> 16) & 0xf;	}

			///
			int offset() const					{	return opcode_ & 0xfff;			}

			// TODO: P, U, B, W, L, I.

		private:
			uint32_t opcode_;

		};

	private:
		uint32_t opcode_;
};

// TODO: do MUL and MLA really transpose Rd and Rn as per the data sheet?
// ARM: Assembly Language Programming by Cockerell thinks not.

}
