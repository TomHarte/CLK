//
//  65816Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef WDC65816Implementation_h
#define WDC65816Implementation_h

enum MicroOp: uint8_t {
	/// Fetches a byte from the program counter to the instruction buffer and increments the program counter.
	CycleFetchIncrementPC,
	/// Fetches a byte from the program counter without incrementing it, and throws it away.
	CycleFetchPC,

	/// Fetches a byte from the data address to the data buffer.
	CycleFetchData,
	/// Fetches a byte from the data address to the data buffer and increments the data address.
	CycleFetchIncrementData,

	/// Stores a byte from the data buffer.
	CycleStoreData,
	/// Stores a byte to the data address from the data buffer and increments the data address.
	CycleStoreIncrementData,
	/// Stores a byte to the data address from the data buffer and decrements the data address.
	CycleStoreDecrementData,

	/// Pushes a single byte from the data buffer to the stack.
	CyclePush,

	/// Sets the data address by copying the final two bytes of the instruction buffer.
	OperationConstructAbsolute,
	/// Sets the data address
	OperationConstructAbsoluteIndexedIndirect,

	/// Performs whatever operation goes with this program.
	OperationPerform,

	/// Copies the current program counter to the data buffer.
	OperationCopyPCToData,

	/// Complete this set of micr-ops.
	OperationMoveToNextProgram
};

enum Operation: uint8_t {
	// These perform the named operation using the value in the data buffer;
	// they are implicitly AccessType::Read.
	ADC, AND, BIT, CMP, CPX, CPY, EOR, ORA, SBC,

	// These load the respective register from the data buffer;
	// they are implicitly AccessType::Read.
	LDA, LDX, LDY,

	// These move the respective register (or value) to the data buffer;
	// they are implicitly AccessType::Write.
	STA, STX, STY, STZ,

	/// Loads the PC with the operand from the data buffer.
	JMP,

	/// Loads the PC and PBR with the operand from the data buffer.
	JML,

	/// Loads the PC with the operand from the daa buffer, replacing
	/// it with the old PC.
	JSR,
};

class ProcessorStorageConstructor;

class ProcessorStorage {
	public:
		ProcessorStorage();

		struct Instruction {
			size_t program_offset;
			Operation operation;
		};
		Instruction instructions[512 + 3];	// Arranged as:
											//	256 entries: emulation-mode instructions;
											//	256 entries: 16-bit instructions;
											//	reset
											//	NMI
											//	IRQ

	private:
		friend ProcessorStorageConstructor;

		// Registers.
		RegisterPair16 a_;
		RegisterPair16 x_, y_;
		uint16_t pc_, s_;

		// Not
		uint16_t direct_;

		// Banking registers are all stored with the relevant byte
		// shifted up bits 16–23.
		uint32_t data_bank_;	// i.e. DBR.
		uint32_t program_bank_;	// i.e. PBR.


		std::vector<MicroOp> micro_ops_;
};

#endif /* WDC65816Implementation_h */
