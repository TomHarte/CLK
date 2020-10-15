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
	/// Fetches a byte from the program counter without incrementing it.
	CycleFetchPC,
	/// Fetches a byte from the program counter without incrementing it, and throws it away.
	CycleFetchPCThrowaway,
	/// The same as CycleFetchIncrementPC but indicates valid program address rather than valid data address.
	CycleFetchOpcode,

	/// Fetches a byte from the data address to the data buffer.
	CycleFetchData,
	/// Fetches a byte from the data address to the data buffer and increments the data address.
	CycleFetchIncrementData,
	/// Fetches from the address formed by the low byte of the data address and the high byte
	/// of the instruction buffer, throwing the result away.
	CycleFetchIncorrectDataAddress,
	/// Fetches a byte from the data address and throws it away.
	CycleFetchDataThrowaway,

	// Dedicated block-move cycles; these use the data buffer as an intermediary.
	CycleFetchBlockX,
	CycleFetchBlockY,
	CycleStoreBlockY,

	/// Stores a byte from the data buffer.
	CycleStoreData,
	/// Stores the most recent byte placed into the data buffer without removing it.
	CycleStoreDataThrowaway,
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
	/// Performs as CyclePull if the 65816 is not in emulation mode; otherwise skips itself.
	CyclePullIfNotEmulation,

	/// Issues a BusOperation::None and regresses the micro-op counter until an established
	/// STP or WAI condition is satisfied.
	CycleRepeatingNone,

	/// Sets the data address by copying the final two bytes of the instruction buffer and
	/// using the data register as a high byte.
	OperationConstructAbsolute,

	/// Constructs a strictly 16-bit address from the instruction buffer.
	OperationConstructAbsolute16,

	/// Sets the data address by copying the entire instruction buffer.
	OperationConstructAbsoluteLong,

	/// Sets the data address to the 16-bit result of adding x to the value in the instruction buffer.
	OperationConstructAbsoluteIndexedIndirect,

	/// Sets the data address to the 24-bit result of adding x to the low 16-bits of the value in the
	/// instruction buffer and retaining the highest 8-bits as specified.
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

	/// Exactly like OperationConstructDirect, but doesn't retain any single-byte wrapping
	/// behaviour in emulation mode.
	OperationConstructDirectLong,

	/// Constructs the current direct indexed indirect address using the data bank,
	/// direct and x registers plus the value currently in the instruction buffer.
	/// Skips the next micro-op if the low byte of the direct register is 0.
	OperationConstructDirectIndexedIndirect,

	/// Constructs the current direct indexed indirect address using the value
	/// currently in the data buffer.
	OperationConstructDirectIndirect,

	/// Adds y to the low 16-bits currently in the instruction buffer and appends a high 8-bits
	/// also from the instruction buffer.
	OperationConstructDirectIndirectIndexedLong,

	/// Uses the 24-bit address currently in the instruction buffer.
	OperationConstructDirectIndirectLong,

	/// Adds the x register to the direct register to produce a 16-bit address;
	/// skips the next micro-op if the low byte of the direct register is 0.
	OperationConstructDirectX,

	/// Adds the y register to the direct register to produce a 16-bit address;
	/// skips the next micro-op if the low byte of the direct register is 0.
	OperationConstructDirectY,

	/// Adds the instruction buffer to the program counter, making a 16-bit result,
	/// *and stores it into the data buffer*.
	OperationConstructPER,

	/// Adds the stack pointer to the instruction buffer to produce a 16-bit address.
	OperationConstructStackRelative,

	/// Adds y to the value in the instruction buffer to produce a 16-bit result and
	/// prefixes the current data bank.
	OperationConstructStackRelativeIndexedIndirect,

	/// Performs whatever operation goes with this program.
	OperationPerform,

	/// Copies the current program counter to the data buffer.
	OperationCopyPCToData,
	OperationCopyDataToPC,
	OperationCopyInstructionToData,
	OperationCopyDataToInstruction,

	/// Copies the current PBR to the data buffer.
	OperationCopyPBRToData,

	/// Copies A to the data buffer.
	OperationCopyAToData,

	/// Copies the data buffer to A.
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
	ADC, AND, BIT, CMP, CPX, CPY, EOR, ORA, SBC, BITimm,

	// These load the respective register from the data buffer;
	// they are implicitly AccessType::Read.
	LDA, LDX, LDY,
	PLB, PLD, PLP,	// LDA, LDX and LDY can be used for PLA, PLX, PLY.

	// These move the respective register (or value) to the data buffer;
	// they are implicitly AccessType::Write.
	STA, STX, STY, STZ,
	PHB, PHP, PHD, PHK,

	// These modify the value in the data buffer as part of a read-modify-write.
	INC, DEC, ASL, LSR, ROL, ROR, TRB, TSB,

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
	RTI,

	/// Loads the PC with the contents of the data buffer.
	JMPind,

	/// Loads the PC with the contents of the instruction bufer.
	JMP,

	/// Loads the PC and PBR with the operand from the instruction buffer.
	JML,

	/// Loads the PC with the operand from the instruction buffer, placing
	/// the current PC into the data buffer.
	JSR,

	/// Loads the PC and the PBR with the operand from the instruction buffer,
	/// placing the old PC into the data buffer (and only the PC; PBR not included).
	JSL,

	/// Loads the PC with the contents of the data buffer + 1.
	RTS,
};

class ProcessorStorageConstructor;

struct ProcessorStorage {
	ProcessorStorage();

	// Frustratingly, there is not quite enough space in 16 bits to store both
	// the program offset and the operation as currently defined.
	struct Instruction {
		/// Pointers into micro_ops_ for: [0] = 16-bit operation; [1] = 8-bit operation.
		uint16_t program_offsets[2] = {0xffff, 0xffff};
		/// The operation to perform upon an OperationPerform.
		Operation operation = NOP;
		/// An index into the mx field indicating which of M or X affects whether this is an 8-bit or 16-bit field.
		/// So the program to perform is that at @c program_offsets[mx_flags[size_field]]
		uint8_t size_field = 0;
	};
	Instruction instructions[256 + 2];	// Arranged as:
										//	256 entries: instructions;
										//	the entry for 'exceptions' (i.e. reset, irq, nmi); and
										//	the entry for fetch-decode-execute.

	enum class OperationSlot {
		Exception = 256,
		FetchDecodeExecute
	};

	// Registers.
	RegisterPair16 a_;
	RegisterPair16 x_, y_;
	RegisterPair16  s_;
	uint16_t pc_;

	// A helper for testing.
	uint16_t last_operation_pc_;
	Instruction *active_instruction_;
	Cycles cycles_left_to_run_;

	// Flags aplenty.
	MOS6502Esque::LazyFlags flags_;
	uint8_t mx_flags_[2] = {1, 1};				// [0] = m; [1] = x. In both cases either `0` or `1`; `1` => 8-bit.
	uint16_t m_masks_[2] = {0xff00, 0x00ff};	// [0] = src mask; [1] = dst mask.
	uint16_t x_masks_[2] = {0xff00, 0x00ff};	// [0] = src mask; [1] = dst mask.
	uint16_t e_masks_[2] = {0xff00, 0x00ff};
	int m_shift_ = 0;
	int x_shift_ = 0;
	bool emulation_flag_ = true;

	// I.e. the offset for direct addressing (outside of emulation mode).
	uint16_t direct_ = 0;

	// Banking registers are all stored with the relevant byte
	// shifted up bits 16–23.
	uint32_t data_bank_ = 0;	// i.e. DBR.
	uint32_t program_bank_ = 0;	// i.e. PBR.

	static constexpr int PowerOn = 1 << 0;
	static constexpr int Reset = 1 << 1;
	static constexpr int IRQ = Flag::Interrupt;	// This makes masking a lot easier later on; this is 1 << 2.
	static constexpr int NMI = 1 << 3;
	int pending_exceptions_ = PowerOn;	// By default.
	int selected_exceptions_ = 0;

	// Just to be safe.
	static_assert(PowerOn != IRQ);
	static_assert(Reset != IRQ);
	static_assert(NMI != IRQ);

	/// Sets the required exception flags necessary to exit a STP or WAI.
	int required_exceptions_ = 0;

	/// Defines a four-byte buffer which can be cleared or filled in single-byte increments from least significant byte
	/// to most significant.
	struct Buffer {
		uint32_t value = 0;
		int size = 0;
		int read = 0;

		void clear() {
			value = 0;
			size = 0;
			read = 0;
		}

		uint8_t *next_input() {
			uint8_t *const next = byte(size);
			++size;
			return next;
		}

		uint8_t *next_output() {
			uint8_t *const next = byte(read);
			++read;
			return next;
		}

		uint8_t *preview_output() {
			return byte(read);
		}

		uint8_t *next_output_descending() {
			--size;
			return byte(size);
		}

		uint8_t *any_byte() {
			return reinterpret_cast<uint8_t *>(&value);
		}

		private:
			uint8_t *byte(int pointer) {
				assert(pointer >= 0 && pointer < 4);
				#if TARGET_RT_BIG_ENDIAN
					return reinterpret_cast<uint8_t *>(&value) + (3 ^ pointer);
				#else
					return reinterpret_cast<uint8_t *>(&value) + pointer;
				#endif
			}
	};
	Buffer instruction_buffer_, data_buffer_;
	uint32_t data_address_;
	uint32_t data_address_increment_mask_ = 0xffff;
	uint32_t incorrect_data_address_;

	std::vector<MicroOp> micro_ops_;
	MicroOp *next_op_ = nullptr;

	void set_reset_state();
	void set_emulation_mode(bool);
	void set_m_x_flags(bool m, bool x);
	uint8_t get_flags() const;
	void set_flags(uint8_t);
};
