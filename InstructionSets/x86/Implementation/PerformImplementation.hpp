//
//  PerformImplementation.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef PerformImplementation_h
#define PerformImplementation_h

#include "../../../Numeric/Carry.hpp"
#include "../../../Numeric/RegisterSizes.hpp"
#include "../Interrupts.hpp"

#include <utility>

namespace InstructionSet::x86 {

template <Model model, typename IntT, typename InstructionT, typename RegistersT, typename MemoryT>
IntT *resolve(
	InstructionT &instruction,
	Source source,
	DataPointer pointer,
	RegistersT &registers,
	MemoryT &memory,
	IntT *none = nullptr,
	IntT *immediate = nullptr
);

template <Model model, Source source, typename IntT, typename InstructionT, typename RegistersT, typename MemoryT>
uint32_t address(
	InstructionT &instruction,
	DataPointer pointer,
	RegistersT &registers,
	MemoryT &memory
) {
	// TODO: non-word indexes and bases.
	if constexpr (source == Source::DirectAddress) {
		return instruction.offset();
	}

	uint32_t address;
	uint16_t zero = 0;
	address = *resolve<model, uint16_t>(instruction, pointer.index(), pointer, registers, memory, &zero);
	if constexpr (is_32bit(model)) {
		address <<= pointer.scale();
	}
	address += instruction.offset();

	if constexpr (source == Source::IndirectNoBase) {
		return address;
	}
	return address + *resolve<model, uint16_t>(instruction, pointer.base(), pointer, registers, memory);
}

template <Model model, typename IntT, typename InstructionT, typename RegistersT, typename MemoryT>
IntT *resolve(
	InstructionT &instruction,
	Source source,
	DataPointer pointer,
	RegistersT &registers,
	MemoryT &memory,
	IntT *none,
	IntT *immediate
) {
	// Rules:
	//
	// * if this is a memory access, set target_address and break;
	// * otherwise return the appropriate value.
	uint32_t target_address;
	switch(source) {
		case Source::eAX:
			// Slightly contorted if chain here and below:
			//
			//	(i) does the `constexpr` version of a `switch`; and
			//	(i) ensures .eax() etc aren't called on @c registers for 16-bit processors, so they need not implement 32-bit storage.
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.eax();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.ax();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.al();		}
			else 																{	return nullptr;				}
		case Source::eCX:
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.ecx();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.cx();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.cl();		}
			else 																{	return nullptr;				}
		case Source::eDX:
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.edx();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.dx();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.dl();		}
			else if constexpr (std::is_same_v<IntT, uint32_t>)					{	return nullptr;				}
		case Source::eBX:
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.ebx();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.bx();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.bl();		}
			else if constexpr (std::is_same_v<IntT, uint32_t>)					{	return nullptr;				}
		case Source::eSPorAH:
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.esp();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.sp();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.ah();		}
			else																{	return nullptr;				}
		case Source::eBPorCH:
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.ebp();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.bp();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.ch();		}
			else 																{	return nullptr;				}
		case Source::eSIorDH:
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.esi();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.si();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.dh();		}
			else 																{	return nullptr;				}
		case Source::eDIorBH:
			if constexpr (is_32bit(model) && std::is_same_v<IntT, uint32_t>) 	{	return &registers.edi();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &registers.di();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &registers.bh();		}
			else																{	return nullptr;				}

		case Source::ES:	if constexpr (std::is_same_v<IntT, uint16_t>) return &registers.es(); else return nullptr;
		case Source::CS:	if constexpr (std::is_same_v<IntT, uint16_t>) return &registers.cs(); else return nullptr;
		case Source::SS:	if constexpr (std::is_same_v<IntT, uint16_t>) return &registers.ss(); else return nullptr;
		case Source::DS:	if constexpr (std::is_same_v<IntT, uint16_t>) return &registers.ds(); else return nullptr;

		// 16-bit models don't have FS and GS.
		case Source::FS:	if constexpr (is_32bit(model) && std::is_same_v<IntT, uint16_t>) return &registers.fs(); else return nullptr;
		case Source::GS:	if constexpr (is_32bit(model) && std::is_same_v<IntT, uint16_t>) return &registers.gs(); else return nullptr;

		case Source::Immediate:
			*immediate = instruction.operand();
		return immediate;

		case Source::None:		return none;

		case Source::Indirect:
			target_address = address<model, Source::Indirect, IntT>(instruction, pointer, registers, memory);
		break;
		case Source::IndirectNoBase:
			target_address = address<model, Source::IndirectNoBase, IntT>(instruction, pointer, registers, memory);
		break;
		case Source::DirectAddress:
			target_address = address<model, Source::DirectAddress, IntT>(instruction, pointer, registers, memory);
		break;
	}

	// If execution has reached here then a memory fetch is required.
	// Do it and exit.
	const Source segment = pointer.segment(instruction.segment_override());
	return &memory.template access<IntT>(segment, target_address);
};

namespace Primitive {

//
// BEGIN TEMPORARY COPY AND PASTE SECTION.
//
// The following are largely excised from the M68k PerformImplementation.hpp; if there proves to be no
// reason further to specialise them, there'll be a factoring out. In some cases I've tightened the documentation.
//

/// @returns An int of type @c IntT with only the most-significant bit set.
template <typename IntT> constexpr IntT top_bit() {
	static_assert(!std::numeric_limits<IntT>::is_signed);
	constexpr IntT max = std::numeric_limits<IntT>::max();
	return max - (max >> 1);
}

/// @returns The number of bits in @c IntT.
template <typename IntT> constexpr int bit_size() {
	return sizeof(IntT) * 8;
}

/// @returns An int with the top bit indicating whether overflow occurred during the calculation of
///		• @c lhs + @c rhs (if @c is_add is true); or
///		• @c lhs - @c rhs (if @c is_add is false)
/// and the result was @c result. All other bits will be clear.
template <bool is_add, typename IntT>
IntT overflow(IntT lhs, IntT rhs, IntT result) {
	const IntT output_changed = result ^ lhs;
	const IntT input_differed = lhs ^ rhs;

	if constexpr (is_add) {
		return top_bit<IntT>() & output_changed & ~input_differed;
	} else {
		return top_bit<IntT>() & output_changed & input_differed;
	}
}
// NOTE TO FUTURE SELF: the original 68k `overflow` treats lhs and rhs the other way
// around, affecting subtractive overflow. Be careful.

//
// END COPY AND PASTE SECTION.
//

//
// Comments below on intended functioning of each operation come from the 1997 edition of the
// Intel Architecture Software Developer’s Manual; that year all such definitions still fitted within a
// single volume, Volume 2.
//
// Order Number 243191; e.g. https://www.ardent-tool.com/CPU/docs/Intel/IA/243191-002.pdf
//

inline void aaa(CPU::RegisterPair16 &ax, Status &status) {	// P. 313
	/*
		IF ((AL AND 0FH) > 9) OR (AF = 1)
			THEN
				AL ← (AL + 6);
				AH ← AH + 1;
				AF ← 1;
				CF ← 1;
			ELSE
				AF ← 0;
				CF ← 0;
			FI;
		AL ← AL AND 0FH;
	*/
	/*
		The AF and CF flags are set to 1 if the adjustment results in a decimal carry;
		otherwise they are cleared to 0. The OF, SF, ZF, and PF flags are undefined.
	*/
	if((ax.halves.low & 0x0f) > 9 || status.auxiliary_carry) {
		ax.halves.low += 6;
		++ax.halves.high;
		status.auxiliary_carry = status.carry = 1;
	} else {
		status.auxiliary_carry = status.carry = 0;
	}
	ax.halves.low &= 0x0f;
}

inline void aad(CPU::RegisterPair16 &ax, uint8_t imm, Status &status) {
	/*
		tempAL ← AL;
		tempAH ← AH;
		AL ← (tempAL + (tempAH * imm8)) AND FFH; (* imm8 is set to 0AH for the AAD mnemonic *)
		AH ← 0
	*/
	/*
		The SF, ZF, and PF flags are set according to the result;
		the OF, AF, and CF flags are undefined.
	*/
	ax.halves.low = ax.halves.low + (ax.halves.high * imm);
	ax.halves.high = 0;
	status.sign = ax.halves.low & 0x80;
	status.parity = status.zero = ax.halves.low;
}

template <typename FlowControllerT>
inline void aam(CPU::RegisterPair16 &ax, uint8_t imm, Status &status, FlowControllerT &flow_controller) {
	/*
		tempAL ← AL;
		AH ← tempAL / imm8; (* imm8 is set to 0AH for the AAD mnemonic *)
		AL ← tempAL MOD imm8;
	*/
	/*
		The SF, ZF, and PF flags are set according to the result.
		The OF, AF, and CF flags are undefined.
	*/
	/*
		If ... an immediate value of 0 is used, it will cause a #DE (divide error) exception.
	*/
	if(!imm) {
		flow_controller.interrupt(Interrupt::DivideError);
		return;
	}

	ax.halves.high = ax.halves.low / imm;
	ax.halves.low = ax.halves.low % imm;
	status.sign = ax.halves.low & 0x80;
	status.parity = status.zero = ax.halves.low;
}

inline void aas(CPU::RegisterPair16 &ax, Status &status) {
	/*
		IF ((AL AND 0FH) > 9) OR (AF = 1)
		THEN
			AL ← AL – 6;
			AH ← AH – 1;
			AF ← 1;
			CF ← 1;
		ELSE
			CF ← 0;
			AF ← 0;
		FI;
		AL ← AL AND 0FH;
	*/
	/*
		The AF and CF flags are set to 1 if there is a decimal borrow;
		otherwise, they are cleared to 0. The OF, SF, ZF, and PF flags are undefined.
	*/
	if((ax.halves.low & 0x0f) > 9 || status.auxiliary_carry) {
		ax.halves.low -= 6;
		--ax.halves.high;
		status.auxiliary_carry = status.carry = 1;
	} else {
		status.auxiliary_carry = status.carry = 0;
	}
	ax.halves.low &= 0x0f;
}

inline void daa(uint8_t &al, Status &status) {
	/*
		(as modified by https://www.felixcloutier.com/x86/daa ...)

        old_AL ← AL;
        old_CF ← CF;
        CF ← 0;

		IF (((AL AND 0FH) > 9) or AF = 1)
			THEN
				AL ← AL + 6;
				CF ← old_CF OR CarryFromLastAddition; (* CF OR carry from AL ← AL + 6 *)
				AF ← 1;
			ELSE
				AF ← 0;
		FI;
		IF ((old_AL > 99H) or old_CF = 1)
			THEN
				AL ← AL + 60H;
				CF ← 1;
			ELSE
				CF ← 0;
		FI;
	*/
	/*
		The CF and AF flags are set if the adjustment of the value results in a
		decimal carry in either digit of the result (see the “Operation” section above).
		The SF, ZF, and PF flags are set according to the result. The OF flag is undefined.
	*/
	const uint8_t old_al = al;
	const auto old_carry = status.carry;
	status.carry = 0;

	if((al & 0x0f) > 0x09 || status.auxiliary_carry) {
		status.carry = old_carry | (al > 0xf9);
		al += 0x06;
		status.auxiliary_carry = 1;
	} else {
		status.auxiliary_carry = 0;
	}

	if(old_al > 0x99 || old_carry) {
		al += 0x60;
		status.carry = 1;
	} else {
		status.carry = 0;
	}

	status.sign = al & 0x80;
	status.zero = status.parity = al;
}

inline void das(uint8_t &al, Status &status) {
	/*
		(as modified by https://www.felixcloutier.com/x86/daa ...)

        old_AL ← AL;
        old_CF ← CF;
        CF ← 0;

		IF (((AL AND 0FH) > 9) or AF = 1)
			THEN
				AL ← AL - 6;
				CF ← old_CF OR CarryFromLastAddition; (* CF OR borrow from AL ← AL - 6 *)
				AF ← 1;
			ELSE
				AF ← 0;
		FI;
		IF ((old_AL > 99H) or old_CF = 1)
			THEN
				AL ← AL - 60H;
				CF ← 1;
			ELSE
				CF ← 0;
		FI;
	*/
	/*
		The CF and AF flags are set if the adjustment of the value results in a
		decimal carry in either digit of the result (see the “Operation” section above).
		The SF, ZF, and PF flags are set according to the result. The OF flag is undefined.
	*/
	const uint8_t old_al = al;
	const auto old_carry = status.carry;
	status.carry = 0;

	if((al & 0x0f) > 0x09 || status.auxiliary_carry) {
		status.carry = old_carry | (al < 0x06);
		al -= 0x06;
		status.auxiliary_carry = 1;
	} else {
		status.auxiliary_carry = 0;
	}

	if(old_al > 0x99 || old_carry) {
		al -= 0x60;
		status.carry = 1;
	} else {
		status.carry = 0;
	}

	status.sign = al & 0x80;
	status.zero = status.parity = al;
}

template <typename IntT>
void adc(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST + SRC + CF;
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination + source + status.carry_bit<IntT>();

	status.carry = Numeric::carried_out<true, bit_size<IntT>() - 1>(destination, source, result);
	status.auxiliary_carry = Numeric::carried_in<4>(destination, source, result);
	status.sign = result & top_bit<IntT>();
	status.zero = status.parity = result;
	status.overflow = overflow<true, IntT>(destination, source, result);

	destination = result;
}

template <typename IntT>
void add(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST + SRC;
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination + source;

	status.carry = Numeric::carried_out<true, bit_size<IntT>() - 1>(destination, source, result);
	status.auxiliary_carry = Numeric::carried_in<4>(destination, source, result);
	status.sign = result & top_bit<IntT>();
	status.zero = status.parity = result;
	status.overflow = overflow<true, IntT>(destination, source, result);

	destination = result;
}

template <typename IntT>
void sbb(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST - (SRC + CF);
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination - source - status.carry_bit<IntT>();

	status.carry = Numeric::carried_out<false, bit_size<IntT>() - 1>(destination, source, result);
	status.auxiliary_carry = Numeric::carried_in<4>(destination, source, result);
	status.sign = result & top_bit<IntT>();
	status.zero = status.parity = result;
	status.overflow = overflow<false, IntT>(destination, source, result);

	destination = result;
}

template <bool write_back, typename IntT>
void sub(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST - SRC;
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination - source;

	status.carry = Numeric::carried_out<false, bit_size<IntT>() - 1>(destination, source, result);
	status.auxiliary_carry = Numeric::carried_in<4>(destination, source, result);
	status.sign = result & top_bit<IntT>();
	status.zero = status.parity = result;
	status.overflow = overflow<false, IntT>(destination, source, result);

	if constexpr (write_back) {
		destination = result;
	}
}

template <typename IntT>
void test(IntT &destination, IntT source, Status &status) {
	/*
		TEMP ← SRC1 AND SRC2;
		SF ← MSB(TEMP);
		IF TEMP = 0
			THEN ZF ← 0;
			ELSE ZF ← 1;
		FI:
		PF ← BitwiseXNOR(TEMP[0:7]);
		CF ← 0;
		OF ← 0;
	*/
	/*
		The OF and CF flags are cleared to 0.
		The SF, ZF, and PF flags are set according to the result (see the “Operation” section above).
		The state of the AF flag is undefined.
	*/
	const IntT result = destination & source;

	status.sign = result & top_bit<IntT>();
	status.zero = result;
	status.carry = status.overflow = 0;
	status.parity = result;
}

template <typename IntT>
void xchg(IntT &destination, IntT &source) {
	/*
		TEMP ← DEST
		DEST ← SRC
		SRC ← TEMP
	*/
	std::swap(destination, source);
}

template <typename IntT>
void mul(IntT &destination_high, IntT &destination_low, IntT source, Status &status) {
	/*
		IF byte operation
			THEN
				AX ← AL * SRC
			ELSE (* word or doubleword operation *)
				IF OperandSize = 16 THEN
					DX:AX ← AX * SRC
				ELSE (* OperandSize = 32 *)
					EDX:EAX ← EAX * SRC
		FI;
	*/
	/*
		The OF and CF flags are cleared to 0 if the upper half of the result is 0;
		otherwise, they are set to 1. The SF, ZF, AF, and PF flags are undefined.
	*/
	destination_high = (destination_low * source) >> (8 * sizeof(IntT));
	destination_low *= source;
	status.overflow = status.carry = destination_high;
}

template <typename IntT>
void imul(IntT &destination_high, IntT &destination_low, IntT source, Status &status) {
	/*
		(as modified by https://www.felixcloutier.com/x86/daa ...)

		IF (OperandSize = 8)
			THEN
				AX ← AL ∗ SRC (* signed multiplication *)
				IF (AX = SignExtend(AL))
					THEN CF = 0; OF = 0;
					ELSE CF = 1; OF = 1;
				FI;
			ELSE IF OperandSize = 16
				THEN
					DX:AX ← AX ∗ SRC (* signed multiplication *)
					IF (DX:AX = SignExtend(AX))
						THEN CF = 0; OF = 0;
						ELSE CF = 1; OF = 1;
					FI;
				ELSE (* OperandSize = 32 *)
					EDX:EAX ← EAX ∗ SRC (* signed multiplication *)
					IF (EDX:EAX = SignExtend(EAX))
						THEN CF = 0; OF = 0;
						ELSE CF = 1; OF = 1;
					FI;
		FI;
	*/
	using sIntT = typename std::make_signed<IntT>::type;
	destination_high = (sIntT(destination_low) * sIntT(source)) >> (8 * sizeof(IntT));
	destination_low = IntT(sIntT(destination_low) * sIntT(source));

	const auto sign_extension = (destination_low & top_bit<IntT>()) ? IntT(~0) : 0;
	status.overflow = status.carry = destination_high != sign_extension;
}

template <typename IntT, typename FlowControllerT>
void div(IntT &destination_high, IntT &destination_low, IntT source, FlowControllerT &flow_controller) {
	/*
		IF SRC = 0
			THEN #DE; (* divide error *)
		FI;
		IF OperandSize = 8 (* word/byte operation *)
			THEN
				temp ← AX / SRC;
				IF temp > FFH
					THEN #DE; (* divide error *) ;
					ELSE
						AL ← temp;
						AH ← AX MOD SRC;
				FI;
		ELSE
			IF OperandSize = 16 (* doubleword/word operation *)
				THEN
					temp ← DX:AX / SRC;
					IF temp > FFFFH
						THEN #DE; (* divide error *) ;
						ELSE
							AX ← temp;
							DX ← DX:AX MOD SRC;
					FI;
				ELSE (* quadword/doubleword operation *)
					temp ← EDX:EAX / SRC;
					IF temp > FFFFFFFFH
					THEN #DE; (* divide error *) ;
					ELSE
						EAX ← temp;
						EDX ← EDX:EAX MOD SRC;
					FI;
			FI;
		FI;
	*/
	/*
		The CF, OF, SF, ZF, AF, and PF flags are undefined.
	*/
	if(!source) {
		flow_controller.interrupt(Interrupt::DivideError);
		return;
	}

	// TEMPORARY HACK. Will not work with DWords.
	const uint32_t dividend = (destination_high << (8 * sizeof(IntT))) + destination_low;
	const auto result = dividend / source;
	if(IntT(result) != result) {
		flow_controller.interrupt(Interrupt::DivideError);
		return;
	}

	destination_low = IntT(result);
	destination_high = dividend % source;
}

template <typename IntT, typename FlowControllerT>
void idiv(IntT &destination_high, IntT &destination_low, IntT source, FlowControllerT &flow_controller) {
	/*
		IF SRC = 0
			THEN #DE; (* divide error *)
		FI;
		IF OperandSize = 8 (* word/byte operation *)
			THEN
				temp ← AX / SRC; (* signed division *)
				IF (temp > 7FH) OR (temp < 80H) (* if a positive result is greater than 7FH or a negative result is less than 80H *)
					THEN #DE; (* divide error *) ;
					ELSE
						AL ← temp;
						AH ← AX MOD SRC;
				FI;
		ELSE
			IF OperandSize = 16 (* doubleword/word operation *)
				THEN
					temp ← DX:AX / SRC; (* signed division *)
					IF (temp > 7FFFH) OR (temp < 8000H) (* if a positive result is greater than 7FFFH or a negative result is less than 8000H *)
						THEN #DE; (* divide error *) ;
						ELSE
							AX ← temp;
							DX ← DX:AX MOD SRC;
					FI;
				ELSE (* quadword/doubleword operation *)
					temp ← EDX:EAX / SRC; (* signed division *)
					IF (temp > 7FFFFFFFH) OR (temp < 80000000H) (* if a positive result is greater than 7FFFFFFFH or a negative result is less than 80000000H *)
						THEN #DE; (* divide error *) ;
						ELSE
							EAX ← temp;
							EDX ← EDX:EAX MOD SRC;
						FI;
			FI;
		FI;
	*/
	/*
		The CF, OF, SF, ZF, AF, and PF flags are undefined.
	*/
	if(!source) {
		flow_controller.interrupt(Interrupt::DivideError);
		return;
	}

	// TEMPORARY HACK. Will not work with DWords.
	using sIntT = typename std::make_signed<IntT>::type;
	const int32_t dividend = (sIntT(destination_high) << (8 * sizeof(IntT))) + destination_low;
	const auto result = dividend / sIntT(source);
	if(sIntT(result) != result) {
		flow_controller.interrupt(Interrupt::DivideError);
		return;
	}

	destination_low = IntT(result);
	destination_high = dividend % sIntT(source);
}

template <typename IntT>
void inc(IntT &destination, Status &status) {
	/*
		DEST ← DEST + 1;
	*/
	/*
		The CF flag is not affected.
		The OF, SF, ZF, AF, and PF flags are set according to the result.
	*/
	++destination;

	status.overflow = destination == top_bit<IntT>();
	status.sign = destination & top_bit<IntT>();
	status.zero = status.parity = destination;
	status.auxiliary_carry = ((destination - 1) ^ destination) & 0x10;
}

template <typename IntT, typename RegistersT, typename FlowControllerT>
inline void jump(bool condition, IntT displacement, RegistersT &registers, FlowControllerT &flow_controller) {
	/*
		IF condition
			THEN
				EIP ← EIP + SignExtend(DEST);
				IF OperandSize = 16
					THEN
						EIP ← EIP AND 0000FFFFH;
				FI;
		FI;
	*/

	// TODO: proper behaviour in 32-bit.
	if(condition) {
		flow_controller.jump(registers.ip() + displacement);
	}
}

template <typename IntT>
void dec(IntT &destination, Status &status) {
	/*
		DEST ← DEST - 1;
	*/
	/*
		The CF flag is not affected.
		The OF, SF, ZF, AF, and PF flags are set according to the result.
	*/
	status.overflow = destination == top_bit<IntT>();

	--destination;

	status.sign = destination & top_bit<IntT>();
	status.zero = status.parity = destination;
	status.auxiliary_carry = ((destination + 1) ^ destination) & 0x10;
}

template <typename IntT>
void and_(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST AND SRC;
	*/
	/*
		The OF and CF flags are cleared; the SF, ZF, and PF flags are set according to the result.
		The state of the AF flag is undefined.
	*/
	destination &= source;

	status.overflow = 0;
	status.carry = 0;
	status.sign = destination & top_bit<IntT>();
	status.zero = status.parity = destination;
}

template <typename IntT>
void or_(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST OR SRC;
	*/
	/*
		The OF and CF flags are cleared; the SF, ZF, and PF flags are set according to the result.
		The state of the AF flag is undefined.
	*/
	destination |= source;

	status.overflow = 0;
	status.carry = 0;
	status.sign = destination & top_bit<IntT>();
	status.zero = status.parity = destination;
}

template <typename IntT>
void xor_(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST XOR SRC;
	*/
	/*
		The OF and CF flags are cleared; the SF, ZF, and PF flags are set according to the result.
		The state of the AF flag is undefined.
	*/
	destination ^= source;

	status.overflow = 0;
	status.carry = 0;
	status.sign = destination & top_bit<IntT>();
	status.zero = status.parity = destination;
}

template <typename IntT>
void neg(IntT &destination, Status &status) {
	/*
		IF DEST = 0
			THEN CF ← 0
			ELSE CF ← 1;
		FI;
		DEST ← –(DEST)
	*/
	/*
		The CF flag cleared to 0 if the source operand is 0; otherwise it is set to 1.
		The OF, SF, ZF, AF, and PF flags are set according to the result.
	*/
	status.auxiliary_carry = Numeric::carried_in<4>(IntT(0), destination, IntT(-destination));

	destination = -destination;

	status.carry = destination;
	status.overflow = destination == top_bit<IntT>();
	status.sign = destination & top_bit<IntT>();
	status.zero = status.parity = destination;
}

template <typename IntT>
void not_(IntT &destination) {
	/*
		DEST ← NOT DEST;
	*/
	/*
		Flags affected: none.
	*/
	destination  = ~destination;
}

template <typename IntT, typename RegistersT, typename FlowControllerT>
inline void call_relative(IntT offset, RegistersT &registers, FlowControllerT &flow_controller) {
	flow_controller.call(registers.ip() + offset);
}

template <typename IntT, typename FlowControllerT>
inline void call_absolute(IntT target, FlowControllerT &flow_controller) {
	flow_controller.call(target);
}

template <Model model, typename InstructionT, typename FlowControllerT, typename RegistersT, typename MemoryT>
void call_far(InstructionT &instruction,
	FlowControllerT &flow_controller,
	RegistersT &registers,
	MemoryT &memory) {

	// TODO: eliminate 16-bit assumption below.
	uint16_t source_address = 0;
	auto pointer = instruction.destination();
	switch(pointer.template source<false>()) {
		default:
		case Source::Immediate:	flow_controller.call(instruction.segment(), instruction.offset());	return;

		case Source::Indirect:
			source_address = address<model, Source::Indirect, uint16_t>(instruction, pointer, registers, memory);
		break;
		case Source::IndirectNoBase:
			source_address = address<model, Source::IndirectNoBase, uint16_t>(instruction, pointer, registers, memory);
		break;
		case Source::DirectAddress:
			source_address = address<model, Source::DirectAddress, uint16_t>(instruction, pointer, registers, memory);
		break;
	}

	const Source source_segment = pointer.segment(instruction.segment_override());

	const uint16_t offset = memory.template access<uint16_t>(source_segment, source_address);
	source_address += 2;
	const uint16_t segment = memory.template access<uint16_t>(source_segment, source_address);
	flow_controller.call(segment, offset);
}

template <typename IntT>
void cbw(IntT &ax) {
	constexpr IntT test_bit = 1 << (sizeof(IntT) * 4 - 1);
	constexpr IntT low_half = (1 << (sizeof(IntT) * 4)) - 1;

	if(ax & test_bit) {
		ax |= ~low_half;
	} else {
		ax &= low_half;
	}
}

template <typename IntT>
void cwd(IntT &dx, IntT ax) {
	dx = ax & top_bit<IntT>() ? IntT(~0) : IntT(0);
}

inline void clc(Status &status) {	status.carry = 0;				}
inline void cld(Status &status) {	status.direction = 0;			}
inline void cli(Status &status) {	status.interrupt = 0;			}	// TODO: quite a bit more in protected mode.
inline void stc(Status &status) {	status.carry = 1;				}
inline void std(Status &status) {	status.direction = 1;			}
inline void sti(Status &status) {	status.interrupt = 1;			}	// TODO: quite a bit more in protected mode.
inline void cmc(Status &status) {	status.carry = !status.carry;	}

}

template <
	Model model,
	DataSize data_size,
	typename InstructionT,
	typename FlowControllerT,
	typename RegistersT,
	typename MemoryT,
	typename IOT
> void perform(
	const InstructionT &instruction,
	Status &status,
	FlowControllerT &flow_controller,
	RegistersT &registers,
	MemoryT &memory,
	[[maybe_unused]] IOT &io
) {
	using IntT = typename DataSizeType<data_size>::type;
	using AddressT = typename AddressT<is_32bit(model)>::type;

	// Establish source() and destination() shorthand to fetch data if necessary.
	IntT immediate;
	auto source = [&]() -> IntT& {
		return *resolve<model, IntT>(
			instruction,
			instruction.source().template source<false>(),
			instruction.source(),
			registers,
			memory,
			nullptr,
			&immediate);
	};
	auto destination = [&]() -> IntT& {
		return *resolve<model, IntT>(
			instruction,
			instruction.destination().template source<false>(),
			instruction.destination(),
			registers,
			memory,
			nullptr,
			&immediate);
	};

	// Guide to the below:
	//
	//	* use hard-coded register names where appropriate;
	//	* return directly if there is definitely no possible write back to RAM;
	//	* otherwise use the source() and destination() lambdas, and break in order to allow a writeback if necessary.
	switch(instruction.operation) {
		default:
			assert(false);

		case Operation::AAA:	Primitive::aaa(registers.axp(), status);											return;
		case Operation::AAD:	Primitive::aad(registers.axp(), instruction.operand(), status);						return;
		case Operation::AAM:	Primitive::aam(registers.axp(), instruction.operand(), status, flow_controller);	return;
		case Operation::AAS:	Primitive::aas(registers.axp(), status);											return;
		case Operation::DAA:	Primitive::daa(registers.al(), status);												return;
		case Operation::DAS:	Primitive::das(registers.al(), status);												return;

		case Operation::CBW:
			if constexpr (data_size == DataSize::Word) {
				Primitive::cbw(registers.ax());
			} else if constexpr (is_32bit(model) && data_size == DataSize::DWord) {
				Primitive::cbw(registers.eax());
			}
		return;
		case Operation::CWD:
			if constexpr (data_size == DataSize::Word) {
				Primitive::cwd(registers.dx(), registers.ax());
			} else if constexpr (data_size == DataSize::DWord) {
				Primitive::cwd(registers.edx(), registers.eax());
			}
		return;

		case Operation::ESC:
		case Operation::NOP:	return;

		case Operation::HLT:	flow_controller.halt();		return;
		case Operation::WAIT:	flow_controller.wait();		return;

		case Operation::ADC:	Primitive::adc(destination(), source(), status);			break;
		case Operation::ADD:	Primitive::add(destination(), source(), status);			break;
		case Operation::SBB:	Primitive::sbb(destination(), source(), status);			break;
		case Operation::SUB:	Primitive::sub<true>(destination(), source(), status);		break;
		case Operation::CMP:	Primitive::sub<false>(destination(), source(), status);		break;
		case Operation::TEST:	Primitive::test(destination(), source(), status);			break;

		// TODO: all the below could call a common registers getter?
		case Operation::MUL:
			if constexpr (data_size == DataSize::Byte) {
				Primitive::mul(registers.ah(), registers.al(), source(), status);
			} else if constexpr (data_size == DataSize::Word) {
				Primitive::mul(registers.dx(), registers.ax(), source(), status);
			} else if constexpr (data_size == DataSize::DWord) {
				Primitive::mul(registers.edx(), registers.eax(), source(), status);
			}
		return;
		case Operation::IMUL_1:
			if constexpr (data_size == DataSize::Byte) {
				Primitive::imul(registers.ah(), registers.al(), source(), status);
			} else if constexpr (data_size == DataSize::Word) {
				Primitive::imul(registers.dx(), registers.ax(), source(), status);
			} else if constexpr (data_size == DataSize::DWord) {
				Primitive::imul(registers.edx(), registers.eax(), source(), status);
			}
		return;
		case Operation::DIV:
			if constexpr (data_size == DataSize::Byte) {
				Primitive::div(registers.ah(), registers.al(), source(), flow_controller);
			} else if constexpr (data_size == DataSize::Word) {
				Primitive::div(registers.dx(), registers.ax(), source(), flow_controller);
			} else if constexpr (data_size == DataSize::DWord) {
				Primitive::div(registers.edx(), registers.eax(), source(), flow_controller);
			}
		return;
		case Operation::IDIV:
			if constexpr (data_size == DataSize::Byte) {
				Primitive::idiv(registers.ah(), registers.al(), source(), flow_controller);
			} else if constexpr (data_size == DataSize::Word) {
				Primitive::idiv(registers.dx(), registers.ax(), source(), flow_controller);
			} else if constexpr (data_size == DataSize::DWord) {
				Primitive::idiv(registers.edx(), registers.eax(), source(), flow_controller);
			}
		return;

		case Operation::INC:	Primitive::inc(destination(), status);		break;
		case Operation::DEC:	Primitive::dec(destination(), status);		break;

		case Operation::AND:	Primitive::and_(destination(), source(), status);		break;
		case Operation::OR:		Primitive::or_(destination(), source(), status);		break;
		case Operation::XOR:	Primitive::xor_(destination(), source(), status);		break;
		case Operation::NEG:	Primitive::neg(source(), status);						break;
		case Operation::NOT:	Primitive::not_(source());								break;

		case Operation::CALLrel:
			Primitive::call_relative(instruction.displacement(), registers, flow_controller);
		return;
		case Operation::CALLabs:
			Primitive::call_absolute(destination(), flow_controller);
		return;
		case Operation::CALLfar:
			Primitive::call_far<model>(instruction, flow_controller, registers, memory);
		return;

		case Operation::JO:
			Primitive::jump(
				status.condition<Status::Condition::Overflow>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNO:
			Primitive::jump(
				!status.condition<Status::Condition::Overflow>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JB:
			Primitive::jump(
				status.condition<Status::Condition::Below>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNB:
			Primitive::jump(
				!status.condition<Status::Condition::Below>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JZ:
			Primitive::jump(
				status.condition<Status::Condition::Zero>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNZ:
			Primitive::jump(
				!status.condition<Status::Condition::Zero>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JBE:
			Primitive::jump(
				status.condition<Status::Condition::BelowOrEqual>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNBE:
			Primitive::jump(
				!status.condition<Status::Condition::BelowOrEqual>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JS:
			Primitive::jump(
				status.condition<Status::Condition::Sign>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNS:
			Primitive::jump(
				!status.condition<Status::Condition::Sign>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JP:
			Primitive::jump(
				!status.condition<Status::Condition::ParityOdd>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNP:
			Primitive::jump(
				status.condition<Status::Condition::ParityOdd>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JL:
			Primitive::jump(
				status.condition<Status::Condition::Less>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNL:
			Primitive::jump(
				!status.condition<Status::Condition::Less>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JLE:
			Primitive::jump(
				status.condition<Status::Condition::LessOrEqual>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;
		case Operation::JNLE:
			Primitive::jump(
				!status.condition<Status::Condition::LessOrEqual>(),
				instruction.displacement(),
				registers,
				flow_controller);
		return;

		case Operation::CLC:	Primitive::clc(status);				return;
		case Operation::CLD:	Primitive::cld(status);				return;
		case Operation::CLI:	Primitive::cli(status);				return;
		case Operation::STC:	Primitive::stc(status);				return;
		case Operation::STD:	Primitive::std(status);				return;
		case Operation::STI:	Primitive::sti(status);				return;
		case Operation::CMC:	Primitive::cmc(status);				return;

		case Operation::XCHG:	Primitive::xchg(destination(), source());	return;
	}

	// Write to memory if required to complete this operation.
	memory.template write_back<IntT>();
}

template <
	Model model,
	typename InstructionT,
	typename FlowControllerT,
	typename RegistersT,
	typename MemoryT,
	typename IOT
> void perform(
	const InstructionT &instruction,
	Status &status,
	FlowControllerT &flow_controller,
	RegistersT &registers,
	MemoryT &memory,
	IOT &io
) {
	// Dispatch to a function just like this that is specialised on data size.
	// Fetching will occur in that specialised function, per the overlapping
	// meaning of register names.
	switch(instruction.operation_size()) {
		case DataSize::Byte:
			perform<model, DataSize::Byte>(instruction, status, flow_controller, registers, memory, io);
		break;
		case DataSize::Word:
			perform<model, DataSize::Word>(instruction, status, flow_controller, registers, memory, io);
		break;
		case DataSize::DWord:
			if constexpr (is_32bit(model)) {
				perform<model, DataSize::DWord>(instruction, status, flow_controller, registers, memory, io);
			}
		break;
		case DataSize::None:
			perform<model, DataSize::None>(instruction, status, flow_controller, registers, memory, io);
		break;
	}
}

}

#endif /* PerformImplementation_h */
