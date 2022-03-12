//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_x86_Instruction_h
#define InstructionSets_x86_Instruction_h

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace InstructionSet {
namespace x86 {

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

	/// Convert byte into word; source will be AL, destination will be AH.
	CBW,
	/// Convert word to double word; source will be AX and destination will be DX.
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
	/// Unsigned multiply; multiplies the source value by AX or AL, storing the result in DX:AX or AX.
	MUL,
	/// Single operand signed multiply; multiplies the source value by AX or AL, storing the result in DX:AX or AX.
	IMUL_1,
	/// Unsigned divide; divide the source value by AX or AL, storing the quotient in AL and the remainder in AH.
	DIV,
	/// Signed divide; divide the source value by AX or AL, storing the quotient in AL and the remainder in AH.
	IDIV,

	/// Increment; source, destination, operand and displacement will be populated appropriately.
	INC,
	/// Decrement; source, destination, operand and displacement will be populated appropriately.
	DEC,

	/// Reads from the port specified by source to the destination.
	IN,
	/// Writes from the port specified by destination from the source.
	OUT,

	// Various jumps; see the displacement to calculate targets.
	JO,	JNO,	JB, JNB,	JE, JNE,	JBE, JNBE,
	JS, JNS,	JP, JNP,	JL, JNL,	JLE, JNLE,

	/// Far call; see the segment() and offset() fields.
	CALLfar,
	/// Relative call; see displacement().
	CALLrel,
	/// Near call.
	CALLabs,
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
	/// Far jump to the indicated segment and offset.
	JMPfar,
	/// Relative jump performed only if CX = 0; see the displacement.
	JPCX,
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

	/// Compare [bytes or words, per operation size]; source and destination implied to be DS:[SI] and ES:[DI].
	CMPS,
	/// Load string; reads from DS:SI into AL or AX, subject to segment override.
	LODS,
	/// Move string; moves a byte or word from DS:SI to ES:DI. If a segment override is provided, it overrides the the source.
	MOVS,
	/// Scan string; reads a byte or word from DS:SI and compares it to AL or AX.
	SCAS,
	/// Store string; store AL or AX to ES:DI.
	STOS,

	// Perform a possibly-conditional loop, decrementing CX. See the displacement.
	LOOP, LOOPE, LOOPNE,

	/// Loads the destination with the source.
	MOV,
	/// Negatives; source and destination point to the same thing, to negative.
	NEG,
	/// Logical NOT;  source and destination point to the same thing, to negative.
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
	RCL,
	/// Rotate the destination right through carry the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	RCR,
	/// Rotate the destination left the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	ROL,
	/// Rotate the destination right the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	ROR,
	/// Arithmetic shift left the destination by the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	SAL,
	/// Arithmetic shift right the destination by the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	SAR,
	/// Logical shift right the destination by the number of bits indicated by source; if the source is a register then implicitly its size is 1.
	SHR,

	/// Clear carry flag; no source or destination provided.
	CLC,
	/// Clear direction flag; no source or destination provided.
	CLD,
	/// Clear interrupt flag; no source or destination provided.
	CLI,
	/// Set carry flag.
	STC,
	/// Set decimal flag.
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

	//
	// 80186 additions.
	//

	/// Checks whether the signed value in the destination register is within the bounds
	/// stored at the location indicated by the source register, which will point to two
	/// 16- or 32-bit words, the first being a signed lower bound and the signed upper.
	/// Raises a bounds exception if not.
	BOUND,


	/// Create stack frame. See operand() for the nesting level and offset()
	/// for the dynamic storage size.
	ENTER,
	/// Procedure exit; copies BP to SP, then pops a new BP from the stack.
	LEAVE,

	/// Inputs a byte, word or double word from the port specified by DX, writing it to
	/// ES:[e]DI and incrementing or decrementing [e]DI as per the
	/// current EFLAGS DF flag.
	INS,
	/// Outputs a byte, word or double word from ES:[e]DI  to the port specified by DX,
	/// incrementing or decrementing [e]DI as per the current EFLAGS DF flag.]
	OUTS,

	/// Pushes all general purpose registers to the stack, in the order:
	/// AX, CX, DX, BX, [original] SP, BP, SI, DI.
	PUSHA,
	/// Pops all general purpose registers from the stack, in the reverse of
	/// the PUSHA order, i.e. DI, SI, BP, [final] SP, BX, DX, CX, AX.
	POPA,

	//
	// 80286 additions.
	//

	// TODO: expand detail on all operations below.

	/// Adjusts requested privilege level.
	ARPL,
	/// Clears the task-switched flag.
	CLTS,
	/// Loads access rights.
	LAR,

	/// Loads the global descriptor table.
	LGDT,
	/// Loads the interrupt descriptor table.
	LIDT,
	/// Loads the local descriptor table.
	LLDT,
	/// Stores the global descriptor table.
	SGDT,
	/// Stores the interrupt descriptor table.
	SIDT,
	/// Stores the local descriptor table.
	SLDT,

	/// Verifies a segment for reading.
	VERR,
	/// Verifies a segment for writing.
	VERW,

	/// Loads the machine status word.
	LMSW,
	/// Stores the machine status word.
	SMSW,
	/// Loads a segment limit
	LSL,
	/// Loads the task register.
	LTR,
	/// Stores the task register.
	STR,

	/// Three-operand form of IMUL; multiply the immediate by the source and write to the destination.
	IMUL_3,

	/// Undocumented (but used); loads all registers, including internal ones.
	LOADALL,

	//
	// 80386 additions.
	//

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

	/// Compare string double word.
	CMPSD,
	/// [Early 80386s only] Insert bit string.
	IBTS,

	/// Convert dword to qword; fills EDX with the sign bit of EAX.
	CDQ,
	/// Convert word to dword; AX will be expanded to fill EAX.
	/// Compare and contrast to CWD which would expand AX to DX:AX.
	CWDE,

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

	// Selectors.
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

enum class Repetition: uint8_t {
	None, RepE, RepNE
};

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
		constexpr ScaleIndexBase() noexcept {}
		constexpr ScaleIndexBase(uint8_t sib) noexcept : sib_(sib) {}
		constexpr ScaleIndexBase(int scale, Source index, Source base) noexcept :
			sib_(uint8_t(
				scale << 6 |
				(int(index != Source::None ? index : Source::eSI) << 3) |
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

		template <bool obscure_indirectNoBase = false> constexpr Source source() const {
			if constexpr (obscure_indirectNoBase) {
				return (source_ >= Source::IndirectNoBase) ? Source::Indirect : source_;
			}
			return source_;
		}

		constexpr int scale() const {
			return sib_.scale();
		}

		constexpr Source index() const {
			return sib_.index();
		}

		template <bool obscure_indirectNoBase = false> constexpr Source base() const {
			if constexpr (obscure_indirectNoBase) {
				return (source_ <= Source::IndirectNoBase) ? Source::None : sib_.base();
			}
			return sib_.base();
		}

	private:
		Source source_ = Source::Indirect;
		ScaleIndexBase sib_;
};

template<bool is_32bit> class Instruction {
	public:
		Operation operation = Operation::Invalid;

		bool operator ==(const Instruction<is_32bit> &rhs) const {
			if(	operation != rhs.operation ||
				mem_exts_source_ != rhs.mem_exts_source_ ||
				source_data_dest_sib_ != rhs.source_data_dest_sib_) {
				return false;
			}

			// Have already established above that this and RHS have the
			// same extensions, if any.
			const int extension_count = has_length_extension() + has_displacement() + has_operand();
			for(int c = 0; c < extension_count; c++) {
				if(extensions_[c] != rhs.extensions_[c]) return false;
			}

			return true;
		}

		using DisplacementT = typename std::conditional<is_32bit, int32_t, int16_t>::type;
		using ImmediateT = typename std::conditional<is_32bit, uint32_t, uint16_t>::type;
		using AddressT = ImmediateT;

	private:
		// Packing and encoding of fields is admittedly somewhat convoluted; what this
		// achieves is that instructions will be sized:
		//
		//	four bytes + up to three extension words
		//	(two bytes for 16-bit instructions, four for 32)
		//
		// Two of the extension words are used to retain an operand and displacement
		// if the instruction has those. The other can store sizes greater than 15
		// bytes (for earlier processors), plus any repetition, segment override or
		// repetition prefixes.

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
		// [b13, b10]: source length (0 => has length extension);
		// [b9, b5]: top five of SIB;
		// [b4, b0]: dest.
		uint16_t source_data_dest_sib_ = 1 << 10;	// So that ::Invalid doesn't seem to have a length extension.

		bool has_length_extension() const {
			return !((source_data_dest_sib_ >> 10) & 15);
		}

		// {operand}, {displacement}, {length extension}.
		//
		// If length extension is present then:
		//
		//	[b15, b6]: source length;
		//	[b5, b4]: repetition;
		//	[b3, b1]: segment override;
		//	b0: lock.
		ImmediateT extensions_[3]{};

		ImmediateT operand_extension() const {
			return extensions_[0];
		}
		ImmediateT displacement_extension() const {
			return extensions_[(mem_exts_source_ >> 5) & 1];
		}
		ImmediateT length_extension() const {
			return extensions_[((mem_exts_source_ >> 5) & 1) + ((mem_exts_source_ >> 6) & 1)];
		}

	public:
		/// @returns The number of bytes used for meaningful content within this class. A receiver must use at least @c sizeof(Instruction) bytes
		/// to store an @c Instruction but is permitted to reuse the trailing sizeof(Instruction) - packing_size() for any purpose it likes. Teleologically,
		/// this allows a denser packing of instructions into containers.
		size_t packing_size() const	{
			return
				offsetof(Instruction<is_32bit>, extensions) +
				(has_displacement() + has_operand() + has_length_extension()) * sizeof(ImmediateT);

			// To consider in the future: the length extension is always the last one,
			// and uses only 8 bits of content within 32-bit instructions, so it'd be
			// possible further to trim the packing size on little endian machines.
			//
			// ... but is that a speed improvement? How much space does it save, and
			// is it enough to undo the costs of unaligned data?
		}

	private:
		// A lookup table to help with stripping parts of the SIB that have been
		// hidden within the source/destination fields.
		static constexpr uint8_t sib_masks[] = {
			0x1f, 0x1f, 0x1f, 0x18
		};

	public:
		DataPointer source() const {
			return DataPointer(
				Source(mem_exts_source_ & sib_masks[(mem_exts_source_ >> 3) & 3]),
				((source_data_dest_sib_ >> 2) & 0xf8) | (mem_exts_source_ & 0x07)
			);
		}
		DataPointer destination() const	{
			return DataPointer(
				Source(source_data_dest_sib_ & sib_masks[(source_data_dest_sib_ >> 3) & 3]),
				((source_data_dest_sib_ >> 2) & 0xf8) | (source_data_dest_sib_ & 0x07)
			);
		}
		bool lock() const {
			return has_length_extension() && length_extension()&1;
		}

		AddressSize address_size() const {
			return AddressSize(mem_exts_source_ >> 7);
		}

		/// @returns @c Source::DS if no segment override was found; the overridden segment otherwise.
		/// On x86 a segment override cannot modify the segment used as a destination in string instructions,
		/// or that used by stack instructions, but this function does not spend the time necessary to provide
		/// the correct default for those.
		Source data_segment() const {
			if(!has_length_extension()) return Source::DS;
			return Source(
				int(Source::ES) +
				((length_extension() >> 1) & 7)
			);
		}

		Repetition repetition() const {
			if(!has_length_extension()) return Repetition::None;
			return Repetition((length_extension() >> 4) & 3);
		}
		DataSize operation_size() const {
			return DataSize(source_data_dest_sib_ >> 14);
		}

		int length() const {
			const int short_length = (source_data_dest_sib_ >> 10) & 15;
			if(short_length) return short_length;
			return length_extension() >> 6;
		}

		ImmediateT operand() const	{
			const ImmediateT ops[] = {0, operand_extension()};
			return ops[has_operand()];
		}
		DisplacementT displacement() const {
			return DisplacementT(offset());
		}

		uint16_t segment() const		{
			return uint16_t(operand());
		}
		ImmediateT offset() const	{
			const ImmediateT offsets[] = {0, displacement_extension()};
			return offsets[has_displacement()];
		}

		constexpr Instruction() noexcept {}
		constexpr Instruction(Operation operation, int length) noexcept :
			Instruction(operation, Source::None, Source::None, ScaleIndexBase(), false, AddressSize::b16, Source::None, Repetition::None, DataSize::None, 0, 0, length) {}
		constexpr Instruction(
			Operation operation,
			Source source,
			Source destination,
			ScaleIndexBase sib,
			bool lock,
			AddressSize address_size,
			Source segment_override,
			Repetition repetition,
			DataSize data_size,
			DisplacementT displacement,
			ImmediateT operand,
			int length) noexcept :
				operation(operation),
				mem_exts_source_(uint8_t(
					(int(address_size) << 7) |
					(displacement ? 0x40 : 0x00) |
					(operand ? 0x20 : 0x00) |
					int(source) |
					(source == Source::Indirect ? (uint8_t(sib) & 7) : 0)
				)),
				source_data_dest_sib_(uint16_t(
					(int(data_size) << 14) |
					((
						(lock || (segment_override != Source::None) || (length > 15) || (repetition != Repetition::None))
					) ? 0 : (length << 10)) |
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
				if(has_length_extension()) {
					// As per the rule stated for segment(), this class provides ::DS for any instruction
					// that doesn't have a segment override.
					if(segment_override == Source::None) segment_override = Source::DS;
					extensions_[extension] = ImmediateT(
						(length << 6) | (int(repetition) << 4) | ((int(segment_override) & 7) << 1) | int(lock)
					);
					++extension;
				}
			}
};

static_assert(sizeof(Instruction<true>) <= 16);
static_assert(sizeof(Instruction<false>) <= 10);

}
}

#endif /* InstructionSets_x86_Instruction_h */
