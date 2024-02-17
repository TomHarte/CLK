//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Model.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace InstructionSet::x86 {

/*
	Operations are documented below to establish expectations as to which
	instruction fields will be meaningful for each; this is a work-in-progress
	and may currently contain errors in the opcode descriptions — especially
	where implicit register dependencies are afoot.
*/
enum class Operation: uint8_t {
	Invalid,

	//
	// 8086 instructions.
	//

	/// ASCII adjust after addition; source will be AL and destination will be AX.
	AAA,
	/// ASCII adjust before division; destination will be AX and source will be a multiplier.
	AAD,
	/// ASCII adjust after multiplication; destination will be AX and source will be a divider.
	AAM,
	/// ASCII adjust after subtraction; source will be AL and destination will be AX.
	AAS,
	/// Decimal adjust after addition; source and destination will be AL.
	DAA,
	/// Decimal adjust after subtraction; source and destination will be AL.
	DAS,

	/// If data size is word, convert byte into word; source will be AL, destination will be AH.
	/// If data size is DWord, convert word to dword; AX will be expanded to fill EAX.
	/// In both cases, conversion will be by sign extension.
	CBW,
	/// If data size is Word, converts word to double word; source will be AX and destination will be DX.
	/// If data size is DWord, converts double word to quad word (i.e. CDW); source will be EAX and destination will be EDX:EAX.
	/// In both cases, conversion will be by sign extension.
	CWD,

	/// Escape, for a coprocessor; perform the bus cycles necessary to read the source and destination and perform a NOP.
	ESC,

	/// Stops the processor until the next interrupt is fired.
	HLT,
	/// Waits until the WAIT input is asserted; if an interrupt occurs then it is serviced but returns to the WAIT.
	WAIT,

	/// Add with carry; source, destination, operand and displacement will be populated appropriately.
	ADC,
	/// Add; source, destination, operand and displacement will be populated appropriately.
	ADD,
	/// Subtract with borrow; source, destination, operand and displacement will be populated appropriately.
	SBB,
	/// Subtract; source, destination, operand and displacement will be populated appropriately.
	SUB,
	/// Unsigned multiply; multiplies the source value by EAX, AX or AL, storing the result in EDX:EAX, DX:AX or AX.
	MUL,
	/// Single operand signed multiply; multiplies the source value by EAX, AX or AL, storing the result in EDX:EAX, DX:AX or AX.
	IMUL_1,
	/// Unsigned divide; divide the AX, DX:AX or EDX:AX by the source(), storing the quotient in AL, AX or EAX and the remainder in AH, DX or EDX.
	DIV,
	/// Signed divide; divide the source value by AX or AL, storing the quotient in AL and the remainder in AH.
	IDIV,

	/// Increment; source, destination, operand and displacement will be populated appropriately.
	INC,
	/// Decrement; source, destination, operand and displacement will be populated appropriately.
	DEC,

	/// Reads from the port specified by source to the destination.
	IN,
	/// Writes to the port specified by destination from the source.
	OUT,

	// Various jumps; see the displacement to calculate targets.
	JO,	JNO,	JB, JNB,	JZ, JNZ,	JBE, JNBE,
	JS, JNS,	JP, JNP,	JL, JNL,	JLE, JNLE,

	/// Near call.
	CALLabs,
	/// Relative call; see displacement().
	CALLrel,
	/// Far call; if destination is Source::Immediate then see the segment() and offset() fields; otherwise take segment and offset by indirection.
	CALLfar,
	/// Return from interrupt.
	IRET,
	/// Near return; if source is not ::None then it will be an ::Immediate indicating how many additional bytes to remove from the stack.
	RETfar,
	/// Far return; if source is not ::None then it will be an ::Immediate indicating how many additional bytes to remove from the stack.
	RETnear,
	/// Near jump with an absolute destination.
	JMPabs,
	/// Near jump with a relative destination.
	JMPrel,
	/// Far jump;  if destination is Source::Immediate then see the segment() and offset() fields; otherwise take segment and offset by indirection.
	JMPfar,
	/// Relative jump performed only if CX = 0; see the displacement.
	JCXZ,
	/// Generates a software interrupt of the level stated in the operand.
	INT,
	/// Generates a software interrupt of level 4 if overflow is set.
	INTO,

	/// Load status flags to AH.
	LAHF,
	/// Load status flags from AH.
	SAHF,
	/// Load a segment and offset from the source into DS and the destination.
	LDS,
	/// Load a segment and offset from the source into ES and the destination.
	LES,
	/// Computes the effective address of the source and loads it into the destination.
	LEA,

	/// Move string; moves a byte or word from DS:SI to ES:DI. If a segment override is provided, it overrides the the source.
	MOVS,
	MOVS_REP,
	/// Load string; reads from DS:SI into AL or AX, subject to segment override.
	LODS,
	LODS_REP,
	/// Store string; store AL or AX to ES:DI.
	STOS,
	STOS_REP,
	/// Compare [bytes or words, per operation size]; source and destination implied to be DS:[SI] and ES:[DI].
	CMPS,
	CMPS_REPE,
	CMPS_REPNE,
	/// Scan string; reads a byte or word from DS:SI and compares it to AL or AX.
	SCAS,
	SCAS_REPE,
	SCAS_REPNE,

	// Perform a possibly-conditional loop, decrementing CX. See the displacement.
	LOOP, LOOPE, LOOPNE,

	/// Loads the destination with the source.
	MOV,
	/// Negatives; source indicates what to negative.
	NEG,
	/// Logical NOT; source indicates what to negative.
	NOT,
	/// Logical AND; source, destination, operand and displacement will be populated appropriately.
	AND,
	/// Logical OR of source onto destination.
	OR,
	/// Logical XOR of source onto destination.
	XOR,
	/// NOP; no further fields.
	NOP,
	/// POP from the stack to destination.
	POP,
	/// POP from the stack to the flags register.
	POPF,
	/// PUSH the source to the stack.
	PUSH,
	/// PUSH the flags register to the stack.
	PUSHF,

	/// Rotate the destination left through carry the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	/// If it is ::None then the rotation is by a single position only.
	RCL,
	/// Rotate the destination right through carry the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	/// If it is ::None then the rotation is by a single position only.
	RCR,
	/// Rotate the destination left the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	/// If it is ::None then the rotation is by a single position only.
	ROL,
	/// Rotate the destination right the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	/// If it is ::None then the rotation is by a single position only.
	ROR,
	/// Arithmetic shift left the destination by the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	/// If it is ::None then the shift is by a single position only.
	SAL,
	/// Arithmetic shift right the destination by the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	/// If it is ::None then the shift is by a single position only.
	SAR,
	/// Logical shift right the destination by the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	/// If it is ::None then the shift is by a single position only.
	SHR,

	/// Clear carry flag; no source or destination provided.
	CLC,
	/// Clear direction flag; no source or destination provided.
	CLD,
	/// Clear interrupt flag; no source or destination provided.
	CLI,
	/// Set carry flag.
	STC,
	/// Set direction flag.
	STD,
	/// Set interrupt flag.
	STI,
	/// Complement carry flag; no source or destination provided.
	CMC,

	/// Compare; source, destination, operand and displacement will be populated appropriately.
	CMP,
	/// Sets flags based on the result of a logical AND of source and destination.
	TEST,

	/// Exchanges the contents of the source and destination.
	XCHG,

	/// Load AL with DS:[AL+BX].
	XLAT,

	/// Set AL to FFh if carry is set; 00h otherwise.
	SALC,

	//
	// 8086 exclusives.
	//

	/// Set destination to ~0 if CL is non-zero.
	SETMOC,
	/// Set destination to ~0.
	SETMO,
	/// Perform an IDIV and negative the result.
	IDIV_REP,

	//
	// 80186 additions.
	//

	/// Checks whether the signed value in the destination register is within the bounds
	/// stored at the location indicated by the source register, which will point to two
	/// 16- or 32-bit words, the first being a signed lower bound and the second
	/// a signed upper.
	/// Raises a bounds exception if not.
	BOUND = SETMOC,

	/// Create stack frame. See the Instruction getters `nesting_level()`
	/// and `dynamic_storage_size()`.
	ENTER,
	/// Procedure exit; copies BP to SP, then pops a new BP from the stack.
	LEAVE,

	/// Inputs a byte, word or double word from the port specified by DX, writing it to
	/// ES:[e]DI and incrementing or decrementing [e]DI as per the
	/// current EFLAGS DF flag.
	INS,
	INS_REP,
	/// Outputs a byte, word or double word from ES:[e]DI to the port specified by DX,
	/// incrementing or decrementing [e]DI as per the current EFLAGS DF flag.
	OUTS,
	OUTS_REP,

	/// Pushes all general purpose registers to the stack, in the order:
	/// AX, CX, DX, BX, [original] SP, BP, SI, DI.
	PUSHA,
	/// Pops all general purpose registers from the stack, in the reverse of
	/// the PUSHA order, i.e. DI, SI, BP, [final] SP, BX, DX, CX, AX.
	POPA,

	//
	// 80286 additions.
	//

	/// Read a segment selector from the destination and one from the source.
	/// If the destination RPL is less than the source, set ZF and set the destination RPL to the source.
	/// Otherwise clear ZF and don't modify the destination selector.
	ARPL,
	/// Clears the task-switched flag in CR0.
	CLTS,
	/// Loads access rights.
	LAR,

	/// Loads the global descriptor table register from the source.
	/// 32-bit operand: read a 16-bit limit followed by a 32-bit base.
	/// 16-bit operand: read a 16-bit limit followed by a 24-bit base.
	LGDT,
	/// Stores the global descriptor table register at the destination;
	/// Always stores a 16-bit limit followed by a 32-bit base though
	/// the highest byte may be zero.
	SGDT,

	/// Loads the interrupt descriptor table register from the source.
	/// 32-bit operand: read a 16-bit limit followed by a 32-bit base.
	/// 16-bit operand: read a 16-bit limit followed by a 24-bit base.
	LIDT,
	/// Stores the interrupt descriptor table register at the destination.
	/// Always stores a 16-bit limit followed by a 32-bit base though
	/// the highest byte may be zero.
	SIDT,

	/// Loads the local descriptor table register.
	/// The source will contain a segment selector pointing into the local descriptor table.
	/// That selector is loaded into the local descriptor table register, along with the corresponding
	///  segment limit and base from the global descriptor table.
	LLDT,
	/// Stores the local descriptor table register.
	SLDT,

	/// Verifies the segment indicated by source for reading, setting ZF.
	///
	/// ZF is set if: (i) the selector is not null; (ii) the selector is within GDT or LDT bounds;
	/// (iii) the selector points to code or data; (iv) the segment is readable;
	/// (v) the segment is either a conforming code segment, or the segment's DPL
	/// is greater than or equal to both the CPL and the selector's RPL.
	///
	/// Otherwise ZF is clear.
	VERR,
	/// Verifies a segment for writing. Operates as per VERR but checks for writeability
	/// rather than readability.
	VERW,

	/// Loads a 16-bit value from source into the machine status word.
	/// The low order four bits of the source are copied into CR0, with the caveat
	/// that if PE is set, the processor will enter protected mode, but if PE is clear
	/// then there will be no change in protected mode.
	///
	/// Usurped in function by MOV CR0 as of the 80286.
	LMSW,
	/// Stores the machine status word, i.e. copies the low 16 bits of CR0 into the destination.
	SMSW,

	/// Load the segment limit from descriptor specified by source into destination,
	/// setting ZF if successful.
	LSL,
	/// Load the source operand into the segment selector field of the task register.
	LTR,
	/// Store the segment seleector of the task register into the destination.
	STR,

	/// Three-operand form of IMUL; multiply the immediate by the source and write to the destination.
	IMUL_3,

	/// Undocumented (but used); loads all registers, including internal ones.
	LOADALL,

	//
	// 80386 additions.
	//

	// TODO: expand detail on all operations below.

	/// Loads a pointer to FS.
	LFS,
	/// Loads a pointer to GS.
	LGS,
	/// Loads a pointer to SS.
	LSS,

	/// Shift left double.
	SHLDimm,
	SHLDCL,
	/// Shift right double.
	SHRDimm,
	SHRDCL,

	/// Bit scan forwards.
	BSF,
	/// Bit scan reverse.
	BSR,
	/// Bit test.
	BT,
	/// Bit test and complement.
	BTC,
	/// Bit test and reset.
	BTR,
	/// Bit test and set.
	BTS,

	/// Move from the source to the destination, extending the source with zeros.
	/// The instruction data size dictates the size of the source; the destination will
	/// be either 16- or 32-bit depending on the current processor operating mode.
	MOVZX,
	/// Move from the source to the destination, applying a sign extension.
	/// The instruction data size dictates the size of the source; the destination will
	/// be either 16- or 32-bit depending on the current processor operating mode.
	MOVSX,

	/// Two-operand form of IMUL; multiply the source by the destination and write to the destination.
	IMUL_2,

	// Various conditional sets; each sets the byte at the location given by the operand
	// to $ff if the condition is met; $00 otherwise.
	SETO, SETNO,	SETB, SETNB,	SETZ, SETNZ,	SETBE, SETNBE,
	SETS, SETNS,	SETP, SETNP,	SETL, SETNL,	SETLE, SETNLE,

	// Various special-case moves (i.e. those where it is impractical to extend the
	// Source enum, so the requirement for special handling is loaded into the operation).
	// In all cases the Cx, Dx and Tx Source aliases can be used to reinterpret the relevant
	// source or destination.
	MOVtoCr, MOVfromCr,
	MOVtoDr, MOVfromDr,
	MOVtoTr, MOVfromTr,
};


enum class DataSize: uint8_t {
	Byte = 0,
	Word = 1,
	DWord = 2,
	None = 3,
};

template <DataSize size> struct DataSizeType { using type = uint8_t; };
template <> struct DataSizeType<DataSize::Word> { using type = uint16_t; };
template <> struct DataSizeType<DataSize::DWord> { using type = uint32_t; };

constexpr int byte_size(DataSize size) {
	return (1 << int(size)) & 7;
}

constexpr int bit_size(DataSize size) {
	return (8 << int(size)) & 0x3f;
}

enum class AddressSize: uint8_t {
	b16 = 0,
	b32 = 1,
};

template <AddressSize size> struct AddressSizeType { using type = uint16_t; };
template <> struct AddressSizeType<AddressSize::b32> { using type = uint32_t; };

constexpr DataSize data_size(AddressSize size) {
	return DataSize(int(size) + 1);
}

constexpr int byte_size(AddressSize size) {
	return 2 << int(size);
}

constexpr int bit_size(AddressSize size) {
	return 16 << int(size);
}

enum class Source: uint8_t {
	// These are in SIB order; this matters for packing later on.

	/// AL, AX or EAX depending on size.
	eAX,
	/// CL, CX or ECX depending on size.
	eCX,
	/// DL, DX or EDX depending on size.
	eDX,
	/// BL, BX or BDX depending on size.
	eBX,
	/// AH if size is 1; SP or ESP otherwise.
	eSPorAH,
	/// CH if size is 1; BP or EBP otherwise.
	eBPorCH,
	/// DH if size is 1; SI or ESI otherwise.
	eSIorDH,
	/// BH if size is 1; DI or EDI otherwise.
	eDIorBH,

	// Aliases for the dual-purpose enums.
	eSP = eSPorAH,	AH = eSPorAH,
	eBP = eBPorCH,	CH = eBPorCH,
	eSI = eSIorDH,	DH = eSIorDH,
	eDI = eDIorBH,	BH = eDIorBH,

	// Aliases for control, test and debug registers.
	C0 = 0, C1 = 1, C2 = 2, C3 = 3, C4 = 4, C5 = 5, C6 = 6, C7 = 7,
	T0 = 0, T1 = 1, T2 = 2, T3 = 3, T4 = 4, T5 = 5, T6 = 6, T7 = 7,
	D0 = 0, D1 = 1, D2 = 2, D3 = 3, D4 = 4, D5 = 5, D6 = 6, D7 = 7,

	// Segment registers.
	ES, CS, SS, DS, FS, GS,

	/// @c None can be treated as a source that produces 0 when encountered;
	/// it is semantically valid to receive it with that meaning in some contexts —
	/// e.g. to indicate no index in indirect addressing.
	/// It's listed here in order to allow an [optional] segment override to fit into three bits.
	None,

	/// The address included within this instruction should be used as the source.
	DirectAddress,

	/// The immediate value included within this instruction should be used as the source.
	Immediate,

	/// The ScaleIndexBase associated with this source should be used.
	Indirect = 0b11000,
	// Elsewhere, as an implementation detail, the low three bits of an indirect source
	// are reused; (Indirect-1) is also used as a sentinel value but is not a valid member
	// of the enum and isn't exposed externally.

	/// The ScaleIndexBase associated with this source should be used, but
	/// its base should be ignored (and is guaranteed to be zero if the default
	/// getter is used).
	IndirectNoBase = Indirect - 1,
};
constexpr bool is_register(Source source) {
	return source < Source::None;
}
constexpr bool is_segment_register(Source source) {
	return is_register(source) && source >= Source::ES;
}

enum class Repetition: uint8_t {
	None, RepE, RepNE, Rep,
};

/// @returns @c true if @c operation supports repetition mode @c repetition; @c false otherwise.
template <Model model>
constexpr Operation rep_operation(Operation operation, Repetition repetition) {
	switch(operation) {
		case Operation::IDIV:
			if constexpr (model == Model::i8086) {
				return repetition != Repetition::None ? Operation::IDIV_REP : Operation::IDIV;
			}
		[[fallthrough]];

		default: return operation;

		case Operation::INS:
			return repetition != Repetition::None ? Operation::INS_REP : Operation::INS;
		case Operation::OUTS:
			return repetition != Repetition::None ? Operation::OUTS_REP : Operation::OUTS;
		case Operation::LODS:
			return repetition != Repetition::None ? Operation::LODS_REP : Operation::LODS;
		case Operation::MOVS:
			return repetition != Repetition::None ? Operation::MOVS_REP : Operation::MOVS;
		case Operation::STOS:
			return repetition != Repetition::None ? Operation::STOS_REP : Operation::STOS;

		case Operation::CMPS:
			switch(repetition) {
				case Repetition::None:	return Operation::CMPS;
				default:
				case Repetition::RepE:	return Operation::CMPS_REPE;
				case Repetition::RepNE:	return Operation::CMPS_REPNE;
			}

		case Operation::SCAS:
			switch(repetition) {
				case Repetition::None:	return Operation::SCAS;
				default:
				case Repetition::RepE:	return Operation::SCAS_REPE;
				case Repetition::RepNE:	return Operation::SCAS_REPNE;
			}
	}
}

/// Provides a 32-bit-style scale, index and base; to produce the address this represents,
/// calcluate base() + (index() << scale()).
///
/// This form of indirect addressing is used to describe both 16- and 32-bit indirect addresses,
/// even though it is a superset of that supported prior to the 80386.
///
/// This class can represent only exactly what a SIB byte can — a scale of 0 to 3, a base
/// that is any one of the eight general purpose registers, and an index that is one of the seven
/// general purpose registers excluding eSP or is ::None.
///
/// It cannot natively describe a base of ::None.
class ScaleIndexBase {
	public:
		constexpr ScaleIndexBase() noexcept = default;
		constexpr ScaleIndexBase(uint8_t sib) noexcept : sib_(sib) {}
		constexpr ScaleIndexBase(int scale, Source index, Source base) noexcept :
			sib_(uint8_t(
				scale << 6 |
				(int(index != Source::None ? index : Source::eSP) << 3) |
				int(base)
			)) {}
		constexpr ScaleIndexBase(Source index, Source base) noexcept : ScaleIndexBase(0, index, base) {}
		constexpr explicit ScaleIndexBase(Source base) noexcept : ScaleIndexBase(0, Source::None, base) {}

		/// @returns the power of two by which to multiply @c index() before adding it to @c base().
		constexpr int scale() const {
			return sib_ >> 6;
		}

		/// @returns the @c index for this address; this is guaranteed to be one of eAX, eBX, eCX, eDX, None, eBP, eSI or eDI.
		constexpr Source index() const {
			constexpr Source sources[] = {
				Source::eAX, Source::eCX, Source::eDX, Source::eBX, Source::None, Source::eBP, Source::eSI, Source::eDI,
			};
			static_assert(sizeof(sources) == 8);
			return sources[(sib_ >> 3) & 0x7];
		}

		/// @returns the @c base for this address; this is guaranteed to be one of eAX, eBX, eCX, eDX, eSP, eBP, eSI or eDI.
		constexpr Source base() const {
			return Source(sib_ & 0x7);
		}

		constexpr uint8_t without_base() const {
			return sib_ & ~0x3;
		}

		bool operator ==(const ScaleIndexBase &rhs) const {
			// Permit either exact equality or index and base being equal
			// but transposed with a scale of 1.
			return
				(sib_ == rhs.sib_) ||
				(
					!scale() &&	!rhs.scale() &&
					rhs.index() == base() &&
					rhs.base() == index()
				);
		}

		operator uint8_t() const {
			return sib_;
		}

	private:
		// Data is stored directly as an 80386 SIB byte.
		uint8_t sib_ = 0;
};
static_assert(sizeof(ScaleIndexBase) == 1);
static_assert(alignof(ScaleIndexBase) == 1);

/// Provides the location of an operand's source or destination.
///
/// Callers should use .source() as a first point of entry. If it directly nominates a register
/// then use the register contents directly. If it indicates ::DirectAddress or ::Immediate
/// then ask the instruction for the address or immediate value that was provided in
/// the instruction.
///
/// If .source() indicates ::Indirect then use base(), index() and scale() to construct an address.
///
/// In all cases, the applicable segment is indicated by the instruction.
class DataPointer {
	public:
		/// Constricts a DataPointer referring to the given source; it shouldn't be ::Indirect.
		constexpr DataPointer(Source source) noexcept : source_(source) {}

		/// Constricts a DataPointer with a source of ::Indirect and the specified sib.
		constexpr DataPointer(ScaleIndexBase sib) noexcept : sib_(sib) {}

		/// Constructs a DataPointer with a source and SIB; use the source to indicate
		/// whether the base field of the SIB is effective.
		constexpr DataPointer(Source source, ScaleIndexBase sib) noexcept : source_(source), sib_(sib) {}

		/// Constructs an indirect DataPointer referencing the given base, index and scale.
		/// Automatically maps Source::Indirect to Source::IndirectNoBase if base is Source::None.
		constexpr DataPointer(Source base, Source index, int scale) noexcept :
			source_(base != Source::None ? Source::Indirect : Source::IndirectNoBase),
			sib_(scale, index, base) {}

		constexpr bool operator ==(const DataPointer &rhs) const {
			// Require a SIB match only if source_ is ::Indirect or ::IndirectNoBase.
			return
				source_ == rhs.source_ && (
					source_ < Source::IndirectNoBase ||
					(source_ == Source::Indirect && sib_ == rhs.sib_) ||
					(source_ == Source::IndirectNoBase && sib_.without_base() == rhs.sib_.without_base())
				);
		}

		constexpr Source source() const {
			return source_;
		}

		constexpr int scale() const {
			return sib_.scale();
		}

		constexpr Source index() const {
			return sib_.index();
		}

		/// @returns The default segment to use for this access.
		constexpr Source default_segment() const {
			switch(source_) {
				default:
				case Source::IndirectNoBase:
					return Source::None;

				case Source::Indirect:
					switch(base()) {
						default:			return Source::DS;
						case Source::eBP:
						case Source::eSP:	return Source::SS;
						case Source::eDI:	return Source::ES;
					}
			}
		}

		constexpr Source base() const {
			return sib_.base();
		}

	private:
		Source source_ = Source::Indirect;
		ScaleIndexBase sib_;
};

template<bool is_32bit> class Instruction {
	public:
		using DisplacementT = typename std::conditional<is_32bit, int32_t, int16_t>::type;
		using ImmediateT = typename std::conditional<is_32bit, uint32_t, uint16_t>::type;
		using AddressT = ImmediateT;

		constexpr Instruction() noexcept = default;
		constexpr Instruction(Operation operation) noexcept :
			Instruction(operation, Source::None, Source::None, ScaleIndexBase(), false, AddressSize::b16, Source::None, DataSize::None, 0, 0) {}
		constexpr Instruction(
			Operation operation,
			Source source,
			Source destination,
			ScaleIndexBase sib,
			bool lock,
			AddressSize address_size,
			Source segment_override,
			DataSize data_size,
			DisplacementT displacement,
			ImmediateT operand) noexcept :
				operation_(operation),
				mem_exts_source_(uint8_t(
					(int(address_size) << 7) |
					(displacement ? 0x40 : 0x00) |
					(operand ? 0x20 : 0x00) |
					int(source) |
					(source == Source::Indirect ? (uint8_t(sib) & 7) : 0)
				)),
				source_data_dest_sib_(uint16_t(
					(int(data_size) << 14) |
					(lock ? (1 << 13) : 0) |
					((uint8_t(sib) & 0xf8) << 2) |
					int(destination) |
					(destination == Source::Indirect ? (uint8_t(sib) & 7) : 0)
				)) {
			// Decisions on whether to include operand, displacement and/or size extension words
			// have implicitly been made in the int packing above; honour them here.
			int extension = 0;
			if(has_operand()) {
				extensions_[extension] = operand;
				++extension;
			}
			if(has_displacement()) {
				extensions_[extension] = ImmediateT(displacement);
				++extension;
			}

			// Patch in a fully-resolved segment.
			Source segment = segment_override;
			if(segment == Source::None) segment = this->source().default_segment();
			if(segment == Source::None) segment = this->destination().default_segment();
			if(segment == Source::None) segment = Source::DS;
			source_data_dest_sib_ |= (int(segment)&7) << 10;
		}

		/// @returns The number of bytes used for meaningful content within this class. A receiver must use at least @c sizeof(Instruction) bytes
		/// to store an @c Instruction but is permitted to reuse the trailing sizeof(Instruction) - packing_size() for any purpose it likes. Teleologically,
		/// this allows a denser packing of instructions into containers.
		constexpr size_t packing_size() const	{
			return
				offsetof(Instruction<is_32bit>, extensions_) +
				(has_displacement() + has_operand()) * sizeof(ImmediateT);
		}

		/// @returns The @c Operation performed by this instruction.
		constexpr Operation operation() const {
			return operation_;
		}

		/// @returns A @c DataPointer describing the 'destination' of this instruction, conventionally the first operand in Intel-syntax assembly.
		constexpr DataPointer destination() const	{
			return DataPointer(
				Source(source_data_dest_sib_ & sib_masks[(source_data_dest_sib_ >> 3) & 3]),
				((source_data_dest_sib_ >> 2) & 0xf8) | (source_data_dest_sib_ & 0x07)
			);
		}

		/// @returns A @c DataPointer describing the 'source' of this instruction, conventionally the second operand in Intel-syntax assembly.
		constexpr DataPointer source() const {
			return DataPointer(
				Source(mem_exts_source_ & sib_masks[(mem_exts_source_ >> 3) & 3]),
				((source_data_dest_sib_ >> 2) & 0xf8) | (mem_exts_source_ & 0x07)
			);
		}

		/// @returns @c true if the lock prefix was present on this instruction; @c false otherwise.
		constexpr bool lock() const {
			return source_data_dest_sib_ & (1 << 13);
		}

		/// @returns The address size for this instruction; will always be 16-bit for instructions decoded by a 16-bit decoder but can be 16- or 32-bit for
		/// instructions decoded by a 32-bit decoder, depending on the program's use of the address size prefix byte.
		constexpr AddressSize address_size() const {
			return AddressSize(mem_exts_source_ >> 7);
		}

		/// @returns The segment that should be used for data fetches if this operation accepts segment overrides.
		constexpr Source data_segment() const {
			return Source(
				int(Source::ES) +
				((source_data_dest_sib_ >> 10) & 7)
			);
		}

		/// @returns The data size of this operation — e.g. `MOV AX, BX` has a data size of `::Word` but `MOV EAX, EBX` has a data size of
		/// `::DWord`. This value is guaranteed never to be `DataSize::None` even for operations such as `CLI` that don't have operands and operate
		/// on data that is not a byte, word or double word.
		constexpr DataSize operation_size() const {
			return DataSize(source_data_dest_sib_ >> 14);
		}

		/// @returns The immediate value provided with this instruction, if any. E.g. `ADD AX, 23h` has the operand `23h`.
		constexpr ImmediateT operand() const	{
			const ImmediateT ops[] = {0, operand_extension()};
			return ops[has_operand()];
		}

		/// @returns The nesting level argument supplied to an ENTER.
		constexpr ImmediateT nesting_level() const	{
			return operand();
		}

		/// @returns The immediate segment value provided with this instruction, if any. Relevant for far calls and jumps; e.g.  `JMP 1234h:5678h` will
		/// have a segment value of `1234h`.
		constexpr uint16_t segment() const		{
			return uint16_t(operand());
		}

		/// @returns The offset provided with this instruction, if any. E.g. `MOV AX, [es:1998h]` has an offset of `1998h`.
		constexpr ImmediateT offset() const	{
			const ImmediateT offsets[] = {0, displacement_extension()};
			return offsets[has_displacement()];
		}

		/// @returns The displacement provided with this instruction `SUB AX, [SI+BP-23h]` has an offset of `-23h` and `JMP 19h`
		/// has an offset of `19h`.
		constexpr DisplacementT displacement() const {
			return DisplacementT(offset());
		}

		/// @returns The dynamic storage size argument supplied to an ENTER.
		constexpr ImmediateT dynamic_storage_size() const	{
			return displacement();
		}

		// Standard comparison operator.
		constexpr bool operator ==(const Instruction<is_32bit> &rhs) const {
			if(	operation_ != rhs.operation_ ||
				mem_exts_source_ != rhs.mem_exts_source_ ||
				source_data_dest_sib_ != rhs.source_data_dest_sib_) {
				return false;
			}

			// Have already established above that this and RHS have the
			// same extensions, if any.
			const int extension_count = has_displacement() + has_operand();
			for(int c = 0; c < extension_count; c++) {
				if(extensions_[c] != rhs.extensions_[c]) return false;
			}

			return true;
		}

	private:
		Operation operation_ = Operation::Invalid;

		// Packing and encoding of fields is admittedly somewhat convoluted; what this
		// achieves is that instructions will be sized:
		//
		//	four bytes + up to two extension words
		//	(extension words being two bytes for 16-bit instructions, four for 32)
		//
		// The extension words are used to retain an operand and displacement
		// if the instruction has those.

		// b7: address size;
		// b6: has displacement;
		// b5: has operand;
		// [b4, b0]: source.
		uint8_t mem_exts_source_ = 0;

		bool has_displacement() const {
			return mem_exts_source_ & (1 << 6);
		}
		bool has_operand() const {
			return mem_exts_source_ & (1 << 5);
		}

		// [b15, b14]: data size;
		// [b13]: lock;
		// [b12, b10]: segment override;
		// [b9, b5]: top five of SIB;
		// [b4, b0]: dest.
		uint16_t source_data_dest_sib_ = 0;

		// {operand}, {displacement}.
		ImmediateT extensions_[2]{};

		ImmediateT operand_extension() const {
			return extensions_[0];
		}
		ImmediateT displacement_extension() const {
			return extensions_[(mem_exts_source_ >> 5) & 1];
		}

		// A lookup table to help with stripping parts of the SIB that have been
		// hidden within the source/destination fields.
		static constexpr uint8_t sib_masks[] = {
			0x1f, 0x1f, 0x1f, 0x18
		};

};

static_assert(sizeof(Instruction<true>) <= 16);
static_assert(sizeof(Instruction<false>) <= 10);

//
// Disassembly aids.
//

/// @returns @c true if @c operation uses a @c displacement().
bool has_displacement(Operation operation);

/// @returns The maximum number of operands to print in a disassembly of @c operation;
///		i.e. 2 for both source() and destination(), 1 for source() alone, 0 for neither. This is a maximum
///		only — if either source is Source::None then it should not be printed.
int max_displayed_operands(Operation operation);

/// Provides the idiomatic name of the @c Operation given an operation @c DataSize and processor @c Model.
std::string to_string(Operation, DataSize, Model);

/// @returns @c true if the idiomatic name of @c Operation implies the data size (e.g. stosb), @c false otherwise (e.g. ld).
bool mnemonic_implies_data_size(Operation);

/// Provides the name of the @c DataSize, i.e. 'byte', 'word' or 'dword'.
std::string to_string(DataSize);

/// Provides the name of the @c Source at @c DataSize, e.g. for Source::eAX it might return AL, AX or EAX.
std::string to_string(Source, DataSize);

/// Provides the printable version of @c pointer as an appendage for @c instruction.
///
/// See notes below re: @c offset_length and @c immediate_length.
/// If @c operation_size is the default value of @c ::None, it'll be taken from the @c instruction.
template <bool is_32bit>
std::string to_string(
	DataPointer pointer,
	Instruction<is_32bit> instruction,
	int offset_length,
	int immediate_length,
	DataSize operation_size = InstructionSet::x86::DataSize::None
);

/// Provides the printable version of @c instruction.
///
/// Internally, instructions do not retain the original sizes of offsets/displacements or immediates so the following are available:
///
/// If @c offset_length is '2' or '4', truncates any printed offset to 2 or 4 digits if it is compatible with being that length.
/// If @c immediate_length is '2' or '4', truncates any printed immediate value to 2 or 4 digits if it is compatible with being that length.
template<bool is_32bit>
std::string to_string(
	std::pair<int, Instruction<is_32bit>> instruction,
	Model model,
	int offset_length = 0,
	int immediate_length = 0);

}
