//
//  65816Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef WDC65816Implementation_h
#define WDC65816Implementation_h

class ProcessorStorage {
	public:
		ProcessorStorage();

		enum MicroOp: uint8_t {
			/// Fetches a byte from the program counter to the instruction buffer and increments the program counter.
			CycleFetchIncrementPC,
			/// Does a no-effect fetch from PC-1.
			CycleFetchPreviousPC,

			/// Fetches a byte from the data address to the data buffer and increments the data address.
			CycleFetchIncrementData,
			/// Stores a byte to the data address from the data buffer and increments the data address.
			CycleStoreIncrementData,

			/// Pushes a single byte from the data buffer to the stack.
			CyclePush,

			/// Skips the next micro-op if in emulation mode.
			OperationSkipIf8,

			/// Sets the data address by copying the final two bytes of the instruction buffer.
			OperationConstructAbsolute,

			/// Performs whatever operation goes with this program.
			OperationPerform,

			/// Complete this set of micr-ops.
			OperationMoveToNextProgram
		};

		enum Operation: uint8_t {
			// These perform the named operation using the value in the data buffer.
			ADC, AND, BIT, CMP, CPX, CPY, EOR, ORA, SBC,

			// These load the respective register from the data buffer.
			LDA, LDX, LDY,

			// These move the respective register (or value) to the data buffer.
			STA, STX, STY, STZ,

			/// Loads the PC with the operand from the instruction buffer.
			JMP,

			/// Loads the PC with the operand from the instruction buffer, placing
			/// the old PC into the data buffer.
			JSR,
		};

		struct Instruction {
			size_t program_offset;
			Operation operation;
		};
		Instruction instructions[256];

	private:
		std::vector<MicroOp> micro_ops_;

		size_t install_ops(std::initializer_list<MicroOp> ops) {
			// Just copy into place and return the index at which copying began.
			const size_t index = micro_ops_.size();
			micro_ops_.insert(micro_ops_.end(), ops.begin(), ops.end());
			return index;
		}

		void set_instruction(uint8_t opcode, size_t micro_ops, Operation operation) {
			instructions[opcode].program_offset = micro_ops;
			instructions[opcode].operation = operation;
		}
};

#endif /* WDC65816Implementation_h */
