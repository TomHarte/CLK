//
//  Decoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Instruction.hpp"
#include "Model.hpp"

#include <cstddef>
#include <utility>

namespace InstructionSet::x86 {

/*!
	Implements Intel x86 instruction decoding.

	This is an experimental implementation; it has not yet undergone significant testing.
*/
template <Model model> class Decoder {
public:
	using InstructionT = Instruction<is_32bit(model)>;

	/*!
		@returns an @c Instruction plus a size; a positive size indicates successful decoding of
			an instruction that was that many bytes long in total; a negative size specifies the [negatived]
			minimum number of further bytes the caller should ideally collect before calling again. The
			caller is free to call with fewer, but may not get a decoded instruction in response, and the
			decoder may still not be able to complete decoding even if given that number of bytes.

			Successful decoding is defined to mean that all decoding steps are complete. The output
			may still be an illegal instruction (indicated by Operation::Invalid), if the byte sequence
			supplied cannot form a valid instruction.

		@discussion although instructions also contain an indicator of their length, on chips prior
			to the 80286 there is no limit to potential instruction length.

			The 80286 and 80386 have instruction length limits of 10 and 15 bytes respectively, so
			cannot overflow the field.
	*/
	std::pair<int, InstructionT> decode(const uint8_t *source, std::size_t length);

	/*!
		Enables or disables 32-bit protected mode. Meaningful only if the @c Model supports it.
	*/
	void set_32bit_protected_mode(bool);

private:
	enum class Phase {
		/// Captures all prefixes and continues until an instruction byte is encountered.
		Instruction,
		/// Having encountered a 0x0f first instruction byte, waits for the next byte fully to determine the instruction.
		InstructionPageF,
		/// Receives a ModRegRM byte and either populates the source_ and dest_ fields appropriately
		/// or completes decoding of the instruction, as per the instruction format.
		ModRegRM,
		/// Awaits n 80386+-style scale-index-base byte ('SIB'), indicating the form of indirect addressing.
		ScaleIndexBase,
		/// Waits for sufficiently many bytes to pass for the required displacement and operand to be captured.
		/// Cf. displacement_size_ and operand_size_.
		DisplacementOrOperand,
		/// Forms and returns an Instruction, and resets parsing state.
		ReadyToPost
	} phase_ = Phase::Instruction;

	/// During the ModRegRM phase, format dictates interpretation of the ModRegRM byte.
	///
	/// During the ReadyToPost phase, format determines how transiently-recorded fields
	/// are packaged into an Instruction.
	enum class ModRegRMFormat: uint8_t {
		// Parse the ModRegRM for mode, register and register/memory fields
		// and populate the source_ and destination_ fields appropriately.
		MemReg_Reg,
		Reg_MemReg,

		// Parse for mode and register/memory fields, populating both
		// source_ and destination_ fields with the single register/memory result.
		MemRegSingleOperand,

		// Parse for mode and register/memory fields, populating both
		// the destination_ field with the result and setting source_ to Immediate.
		MemRegMOV,

		// Parse for mode and register/memory fields, populating the
		// source_ field with the result. Fills destination_ with a segment
		// register based on the reg field.
		Seg_MemReg,
		MemReg_Seg,

		//
		//	'Group 1'
		//

		// Parse for mode and register/memory fields, populating the
		// destination_ field with the result. Use the 'register' field
		// to pick an operation from the ADD/OR/ADC/SBB/AND/SUB/XOR/CMP group and
		// waits for an operand equal to the operation size.
		MemRegADD_to_CMP,

		// Acts exactly as MemRegADD_to_CMP but the operand is fixed in size
		// at a single byte, which is sign extended to the operation size.
		MemRegADD_to_CMP_SignExtend,

		//
		//	'Group 2'
		//

		// Parse for mode and register/memory fields, populating the
		// destination_ field with the result. Use the 'register' field
		// to pick an operation from the ROL/ROR/RCL/RCR/SAL/SHR/SAR group.
		MemRegROL_to_SAR,

		//
		//	'Group 3'
		//

		// Parse for mode and register/memory fields, populating both
		// source_ and destination_ fields with the result. Use the 'register'
		// field to pick an operation from the TEST/NOT/NEG/MUL/IMUL/DIV/IDIV group.
		MemRegTEST_to_IDIV,

		//
		//	'Group 4'
		//

		// Parse for mode and register/memory fields, populating the
		// source_ and destination_ fields with the result. Uses the
		// 'register' field to pick INC or DEC.
		MemRegINC_DEC,

		//
		//	'Group 5'
		//

		// Parse for mode and register/memory fields, populating the
		// source_ and destination_ fields with the result. Uses the
		// 'register' field to pick from INC/DEC/CALL/JMP/PUSH, altering
		// the source to ::Immediate and setting an operand size if necessary.
		MemRegINC_to_PUSH,

		//
		//	'Group 6'
		//

		// Parse for mode and register/memory field, populating both source_
		// and destination_ fields with the result. Uses the 'register' field
		// to pick from SLDT/STR/LLDT/LTR/VERR/VERW.
		MemRegSLDT_to_VERW,

		//
		//	'Group 7'
		//

		// Parse for mode and register/memory field, populating both source_
		// and destination_ fields with the result. Uses the 'register' field
		// to pick from SGDT/LGDT/SMSW/LMSW.
		MemRegSGDT_to_LMSW,

		//
		//	'Group 8'
		//

		// Parse for mode and register/memory field, populating destination,
		// and prepare to read a single byte as source.
		MemRegBT_to_BTC,
	} modregrm_format_ = ModRegRMFormat::MemReg_Reg;

	// Ephemeral decoding state.
	Operation operation_ = Operation::Invalid;
	int consumed_ = 0, operand_bytes_ = 0;

	// Source and destination locations.
	Source source_ = Source::None;
	Source destination_ = Source::None;

	// Immediate fields.
	int32_t displacement_ = 0;
	uint32_t operand_ = 0;
	uint64_t inward_data_ = 0;
	int next_inward_data_shift_ = 0;

	// Indirection style.
	ScaleIndexBase sib_;

	// Facts about the instruction.
	DataSize displacement_size_ = DataSize::None;	// i.e. size of in-stream displacement, if any.
	DataSize operand_size_ = DataSize::None;		// i.e. size of in-stream operand, if any.
	DataSize operation_size_ = DataSize::None;		// i.e. size of data manipulated by the operation.

	bool sign_extend_displacement_ = true;			// If set then sign extend any displacement up to the address
													// size; otherwise it'll be zero-padded.
	bool sign_extend_operand_ = false;				// If set then sign extend the operand up to the operation size;
													// otherwise it'll be zero-padded.

	// Prefix capture fields.
	Repetition repetition_ = Repetition::None;
	bool lock_ = false;
	Source segment_override_ = Source::None;

	// 32-bit/16-bit selection.
	AddressSize default_address_size_ = AddressSize::b16;
	DataSize default_data_size_ = DataSize::Word;
	AddressSize address_size_ = AddressSize::b16;
	DataSize data_size_ = DataSize::Word;

	/// Resets size capture and all fields with default values.
	void reset_parsing() {
		consumed_ = operand_bytes_ = 0;
		displacement_size_ = operand_size_ = operation_size_ = DataSize::None;
		displacement_ = operand_ = 0;
		lock_ = false;
		address_size_ = default_address_size_;
		data_size_ = default_data_size_;
		segment_override_ = Source::None;
		repetition_ = Repetition::None;
		phase_ = Phase::Instruction;
		source_ = destination_ = Source::None;
		sib_ = ScaleIndexBase();
		next_inward_data_shift_ = 0;
		inward_data_ = 0;
		sign_extend_operand_ = false;
		sign_extend_displacement_ = true;
	}

	//
	// Construction helpers.
	//

	/// Sets the operation and verifies that the current repetition, if any, is compatible, discarding it otherwise.
	void set(const Operation operation) {
		operation_ = rep_operation<model>(operation, repetition_);
	};

	/// Helper for those that follow...
	void set(
		const Operation operation,
		const Source source,
		const Source destination,
		const DataSize operation_size
	) {
		set(operation);
		source_ = source;
		destination_ = destination;
		operation_size_ = operation_size;
	};

	/// Covers anything which is complete as soon as the opcode is encountered.
	void complete(
		const Operation operation,
		const Source source,
		const Source destination,
		const DataSize operation_size
	) {
		set(operation, source, destination, operation_size);
		phase_ = Phase::ReadyToPost;
	}

	/// Handles instructions of the form rr, kk and rr, jjkk, i.e. a destination register plus an operand.
	void reg_data(
		const Operation operation,
		const Source destination,
		const DataSize operation_size
	) {
		set(operation, Source::Immediate, destination, operation_size);
		operand_size_ = operation_size;
		phase_ = Phase::DisplacementOrOperand;
	}

	/// Handles instructions of the form Ax, jjkk where the latter is implicitly an address.
	void reg_addr(
		const Operation operation,
		const Source destination,
		const DataSize operation_size,
		const DataSize address_size
	) {
		set(operation, Source::DirectAddress, destination, operation_size);
		displacement_size_ = address_size;
		phase_ = Phase::DisplacementOrOperand;
		sign_extend_displacement_ = false;
	}

	/// Handles instructions of the form jjkk, Ax where the former is implicitly an address.
	void addr_reg(
		const Operation operation,
		const Source source,
		const DataSize operation_size,
		const DataSize address_size
	) {
		set(operation, source, Source::DirectAddress, operation_size);
		displacement_size_ = address_size;
		phase_ = Phase::DisplacementOrOperand;
		sign_extend_displacement_ = false;
	}

	/// Covers both `mem/reg, reg` and `reg, mem/reg`.
	void mem_reg_reg(
		const Operation operation,
		ModRegRMFormat format,
		const DataSize operation_size
	) {
		set(operation);
		phase_ = Phase::ModRegRM;
		modregrm_format_ = format;
		operand_size_ = DataSize::None;
		operation_size_ = operation_size;
	}

	/// Handles JO, JNO, JB, etc — anything with only a displacement.
	void displacement(
		const Operation operation,
		const DataSize operation_size
	) {
		set(operation);
		phase_ = Phase::DisplacementOrOperand;
		operation_size_ = displacement_size_ = operation_size;
	}

	/// Handles PUSH [immediate], etc — anything with only an immediate operand.
	void immediate(
		const Operation operation,
		const DataSize operand_size
	) {
		set(operation);
		source_ = Source::Immediate;
		phase_ = Phase::DisplacementOrOperand;
		operand_size_ = operand_size;
	}

	/// Handles far CALL and far JMP — fixed four or six byte operand operations.
	void far(
		const Operation operation
	) {
		set(operation);
		phase_ = Phase::DisplacementOrOperand;
		operation_size_ = operand_size_ = DataSize::Word;
		destination_ = Source::Immediate;
		displacement_size_ = data_size(default_address_size_);
	}

	/// Handles ENTER — a fixed three-byte operation.
	void word_displacement_byte_operand(
		const Operation operation
	) {
		set(operation);
		phase_ = Phase::DisplacementOrOperand;
		displacement_size_ = DataSize::Word;
		operand_size_ = DataSize::Byte;
	}

	/// Sets up the operation size, oncoming phase and modregrm format for a member of the shift group (i.e. 'group 2').
	void shift_group(
		const int size_mask
	) {
		const DataSize sizes[] = {DataSize::Byte, data_size_};
		phase_ = Phase::ModRegRM;
		modregrm_format_ = ModRegRMFormat::MemRegROL_to_SAR;
		operation_size_ = sizes[size_mask];
	}

	/// Constructs an instruction for 'undefined' and returns it.
	std::pair<int, typename Decoder<model>::InstructionT> undefined() {
		const auto result = std::make_pair(consumed_, InstructionT());
		reset_parsing();
		return result;
	}
};

// As the name implies, this function exists purely to work around what appears
// at my level of comprehension to be an issue with GCC and explicit instantiation.
void _gcc_instantiation_workaround();

}
