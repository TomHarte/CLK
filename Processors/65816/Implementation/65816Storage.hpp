//
//  65816Implementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

enum MicroOp: uint8_t {
	/// Fetches a byte from the program counter to the instruction buffer and increments the program counter.
	CycleFetchIncrementPC,
	/// Fetches a byte from the program counter without incrementing it, and throws it away.
	CycleFetchPC,

	/// Fetches a byte from the data address to the data buffer.
	CycleFetchData,
	/// Fetches a byte from the data address to the data buffer and increments the data address.
	CycleFetchIncrementData,
	/// Fetches from the address formed by the low byte of the data address and the high byte
	/// of the instruction buffer, throwing the result away.
	CycleFetchIncorrectDataAddress,

	/// Fetches a vector (i.e. IRQ, NMI, etc) into the data buffer.
	CycleFetchVector,

	// Dedicated block-move cycles; these use the data buffer as an intermediary.
	CycleFetchBlockX,
	CycleFetchBlockY,
	CycleStoreBlockY,

	/// Stores a byte from the data buffer.
	CycleStoreData,
	/// Stores a byte to the data address from the data buffer and increments the data address.
	CycleStoreIncrementData,
	/// Stores a byte to the data address from the data buffer and decrements the data address.
	CycleStoreDecrementData,

	/// Pushes a single byte from the data buffer to the stack.
	CyclePush,
	/// Fetches from the current stack location and throws the result away.
	CycleAccessStack,
	/// Pulls a single byte to the data buffer from the stack.
	CyclePull,

	/// Sets the data address by copying the final two bytes of the instruction buffer.
	OperationConstructAbsolute,
	/// Sets the data address to the result of (a, x).
	/// TODO: explain better once implemented.
	OperationConstructAbsoluteIndexedIndirect,
	OperationConstructAbsoluteLongX,

	/// Calculates an a, x address; if:
	/// 	there was no carry into the top byte of the address; and
	/// 	the process or in emulation or 8-bit index mode;
	/// then it also skips the next micro-op.
	OperationConstructAbsoluteXRead,

	/// Calculates an a, x address.
	OperationConstructAbsoluteX,

	// These are analogous to the X versions above.
	OperationConstructAbsoluteY,
	OperationConstructAbsoluteYRead,

	/// Constructs the current direct address using the value in the instruction buffer.
	/// Skips the next micro-op if the low byte of the direct register is 0.
	OperationConstructDirect,

	// These follow similar skip-one-if-possible logic to OperationConstructDirect.
	OperationConstructDirectIndexedIndirect,
	OperationConstructDirectIndirect,
	OperationConstructDirectIndirectIndexed,
	OperationConstructDirectIndirectIndexedLong,
	OperationConstructDirectIndirectLong,
	OperationConstructDirectX,
	OperationConstructDirectY,

	OperationConstructPER,
	OperationConstructBRK,

	OperationConstructStackRelative,
	OperationConstructStackRelativeIndexedIndirect,

	/// Performs whatever operation goes with this program.
	OperationPerform,

	/// Copies the current program counter to the data buffer.
	OperationCopyPCToData,
	OperationCopyInstructionToData,

	/// Copies the current PBR to the data buffer.
	OperationCopyPBRToData,

	OperationCopyAToData,
	OperationCopyDataToA,

	/// Fills the data buffer with three or four bytes, depending on emulation mode, containing the program
	/// counter, flags and possibly the program bank. Also puts the appropriate vector address into the
	/// address register.
	OperationPrepareException,

	/// Complete this set of micr-ops.
	OperationMoveToNextProgram,

	/// Inspects the instruction buffer and thereby selects the next set of micro-ops to schedule.
	OperationDecode,
};

enum Operation: uint8_t {
	// These perform the named operation using the value in the data buffer;
	// they are implicitly AccessType::Read.
	ADC, AND, BIT, CMP, CPX, CPY, EOR, ORA, SBC,

	// These load the respective register from the data buffer;
	// they are implicitly AccessType::Read.
	LDA, LDX, LDY,
	PLB, PLD, PLP,	// LDA, LDX and LDY can be used for PLA, PLX, PLY.

	// These move the respective register (or value) to the data buffer;
	// they are implicitly AccessType::Write.
	STA, STX, STY, STZ,
	PHB, PHP, PHD, PHK,

	// These modify the value in the data buffer as part of a read-modify-write.
	ASL, DEC, INC, LSR, ROL, ROR, TRB, TSB,

	// These merely decrement A, increment or decrement X and Y, and regress
	// the program counter only if appropriate.
	MVN, MVP,

	// These use a value straight from the instruction buffer.
	REP, SEP,

	BCC, BCS, BEQ, BMI, BNE, BPL, BRA, BVC, BVS, BRL,

	// These are all implicit.
	CLC, CLD, CLI, CLV, DEX, DEY, INX, INY, NOP, SEC, SED, SEI,
	TAX, TAY, TCD, TCS, TDC, TSC, TSX, TXA, TXS, TXY, TYA, TYX,
	XCE, XBA,

	STP, WAI,

	// These unpack values from the data buffer, which has been filled
	// from the stack.
	RTI, RTL,

	/// Loads the PC with the operand from the data buffer.
	JMP,

	/// Loads the PC and PBR with the operand from the data buffer.
	JML,

	/// Loads the PC with the operand from the data buffer, replacing
	/// it with the old PC.
	JSR,

	/// Loads the PC and the PBR with the operand from the data buffer,
	/// replacing it with the old PC (and only the PC; PBR not included).
	JSL,

	/// i.e. jump to vector. TODO: is this really distinct from JMP? I'm assuming so for now,
	/// as I assume the PBR is implicitly modified. We'll see.
	BRK,
};

class ProcessorStorageConstructor;

struct ProcessorStorage {
	ProcessorStorage();

	// Frustratingly, there is not quite enough space in 16 bits to store both
	// the program offset and the operation as currently defined.
	struct Instruction {
		uint16_t program_offset;
		Operation operation;
	};
	Instruction instructions[514];	// Arranged as:
									//	256 entries: emulation-mode instructions;
									//	256 entries: 16-bit instructions;
									//	the entry for 'exceptions' (i.e. reset, irq, nmi); and
									//	the entry for fetch-decode-execute.

	enum class OperationSlot {
		Exception = 512,
		FetchDecodeExecute
	};

	// Registers.
	RegisterPair16 a_;
	RegisterPair16 x_, y_;
	uint16_t pc_, s_;

	// A helper for testing.
	uint16_t last_operation_pc_;
	Instruction *active_instruction_;
	Cycles cycles_left_to_run_;

	// I.e. the offset for direct addressing (outside of emulation mode).
	uint16_t direct_ = 0;

	// Banking registers are all stored with the relevant byte
	// shifted up bits 16–23.
	uint32_t data_bank_ = 0;	// i.e. DBR.
	uint32_t program_bank_ = 0;	// i.e. PBR.

	static constexpr int PowerOn = 1 << 0;
	static constexpr int Reset = 1 << 1;
	static constexpr int IRQ = 1 << 2;
	static constexpr int NMI = 1 << 3;
	int pending_exceptions_ = PowerOn;	// By default.

	/// Defines a four-byte buffer which can be cleared or filled in single-byte increments from least significant byte
	/// to most significant.
	struct Buffer {
		uint32_t value = 0;
		int size = 0;

		void clear() {
			value = 0;
			size = 0;
		}

		uint8_t *next() {
			#if TARGET_RT_BIG_ENDIAN
			uint8_t *const target = reinterpret_cast<uint8_t *>(&value) + (3 ^ size);
			#else
			uint8_t *const target = reinterpret_cast<uint8_t *>(&value) + size;
			#endif

			++size;
			return target;
		}
	};
	Buffer instruction_buffer_, data_buffer_;

	std::vector<MicroOp> micro_ops_;
	MicroOp *next_op_ = nullptr;
};
