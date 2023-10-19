//
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
uint32_t address(
	InstructionT &instruction,
	DataPointer pointer,
	RegistersT &registers,
	MemoryT &memory
) {
	switch(pointer.source<false>()) {
		default:						return 0;
		case Source::Indirect:			return address<model, Source::Indirect, IntT>(instruction, pointer, registers, memory);
		case Source::IndirectNoBase:	return address<model, Source::IndirectNoBase, IntT>(instruction, pointer, registers, memory);
		case Source::DirectAddress:		return address<model, Source::DirectAddress, IntT>(instruction, pointer, registers, memory);
	}
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

// The below takes a reference in order properly to handle PUSH SP, which should place the value of SP after the
// push onto the stack.
template <typename IntT, typename MemoryT, typename RegistersT>
void push(IntT &value, MemoryT &memory, RegistersT &registers) {
	registers.sp_ -= sizeof(IntT);
	memory.template access<IntT>(
		InstructionSet::x86::Source::SS,
		registers.sp_) = value;
	memory.template write_back<IntT>();
}

template <typename IntT, typename MemoryT, typename RegistersT>
IntT pop(MemoryT &memory, RegistersT &registers) {
	const auto value = memory.template access<IntT>(
		InstructionSet::x86::Source::SS,
		registers.sp_);
	registers.sp_ += sizeof(IntT);
	return value;
}

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
	if((ax.halves.low & 0x0f) > 9 || status.flag<Flag::AuxiliaryCarry>()) {
		ax.halves.low += 6;
		++ax.halves.high;
		status.set_from<Flag::Carry, Flag::AuxiliaryCarry>(1);
	} else {
		status.set_from<Flag::Carry, Flag::AuxiliaryCarry>(0);
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
	status.set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(ax.halves.low);
}

template <typename FlowControllerT>
void aam(CPU::RegisterPair16 &ax, uint8_t imm, Status &status, FlowControllerT &flow_controller) {
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
	status.set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(ax.halves.low);
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
	if((ax.halves.low & 0x0f) > 9 || status.flag<Flag::AuxiliaryCarry>()) {
		ax.halves.low -= 6;
		--ax.halves.high;
		status.set_from<Flag::Carry, Flag::AuxiliaryCarry>(1);
	} else {
		status.set_from<Flag::Carry, Flag::AuxiliaryCarry>(0);
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
	const auto old_carry = status.flag<Flag::Carry>();
	status.set_from<Flag::Carry>(0);

	if((al & 0x0f) > 0x09 || status.flag<Flag::AuxiliaryCarry>()) {
		status.set_from<Flag::Carry>(old_carry | (al > 0xf9));
		al += 0x06;
		status.set_from<Flag::AuxiliaryCarry>(1);
	} else {
		status.set_from<Flag::AuxiliaryCarry>(0);
	}

	if(old_al > 0x99 || old_carry) {
		al += 0x60;
		status.set_from<Flag::Carry>(1);
	} else {
		status.set_from<Flag::Carry>(0);
	}

	status.set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(al);
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
	const auto old_carry = status.flag<Flag::Carry>();
	status.set_from<Flag::Carry>(0);

	if((al & 0x0f) > 0x09 || status.flag<Flag::AuxiliaryCarry>()) {
		status.set_from<Flag::Carry>(old_carry | (al < 0x06));
		al -= 0x06;
		status.set_from<Flag::AuxiliaryCarry>(1);
	} else {
		status.set_from<Flag::AuxiliaryCarry>(0);
	}

	if(old_al > 0x99 || old_carry) {
		al -= 0x60;
		status.set_from<Flag::Carry>(1);
	} else {
		status.set_from<Flag::Carry>(0);
	}

	status.set_from<uint8_t, Flag::Zero, Flag::Sign, Flag::ParityOdd>(al);
}

template <bool with_carry, typename IntT>
void add(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST + SRC [+ CF];
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination + source + (with_carry ? status.carry_bit<IntT>() : 0);

	status.set_from<Flag::Carry>(
		Numeric::carried_out<true, Numeric::bit_size<IntT>() - 1>(destination, source, result));
	status.set_from<Flag::AuxiliaryCarry>(
		Numeric::carried_in<4>(destination, source, result));
	status.set_from<Flag::Overflow>(
		Numeric::overflow<true, IntT>(destination, source, result));

	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(result);

	destination = result;
}

template <bool with_borrow, bool write_back, typename IntT>
void sub(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST - (SRC [+ CF]);
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination - source - (with_borrow ? status.carry_bit<IntT>() : 0);

	status.set_from<Flag::Carry>(
		Numeric::carried_out<false, Numeric::bit_size<IntT>() - 1>(destination, source, result));
	status.set_from<Flag::AuxiliaryCarry>(
		Numeric::carried_in<4>(destination, source, result));
	status.set_from<Flag::Overflow>(
		Numeric::overflow<false, IntT>(destination, source, result));

	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(result);

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

	status.set_from<Flag::Carry, Flag::Overflow>(0);
	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(result);
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
	status.set_from<Flag::Overflow, Flag::Carry>(destination_high);
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

	const auto sign_extension = (destination_low & Numeric::top_bit<IntT>()) ? IntT(~0) : 0;
	status.set_from<Flag::Overflow, Flag::Carry>(destination_high != sign_extension);
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

	status.set_from<Flag::Overflow>(destination == Numeric::top_bit<IntT>());
	status.set_from<Flag::AuxiliaryCarry>(((destination - 1) ^ destination) & 0x10);
	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
}

template <typename IntT, typename RegistersT, typename FlowControllerT>
void jump(bool condition, IntT displacement, RegistersT &registers, FlowControllerT &flow_controller) {
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

template <typename IntT, typename OffsetT, typename RegistersT, typename FlowControllerT>
void loop(IntT &counter, OffsetT displacement, RegistersT &registers, FlowControllerT &flow_controller) {
	--counter;
	if(counter) {
		flow_controller.jump(registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename RegistersT, typename FlowControllerT>
void loope(IntT &counter, OffsetT displacement, RegistersT &registers, Status &status, FlowControllerT &flow_controller) {
	--counter;
	if(counter && status.flag<Flag::Zero>()) {
		flow_controller.jump(registers.ip() + displacement);
	}
}

template <typename IntT, typename OffsetT, typename RegistersT, typename FlowControllerT>
void loopne(IntT &counter, OffsetT displacement, RegistersT &registers, Status &status, FlowControllerT &flow_controller) {
	--counter;
	if(counter && !status.flag<Flag::Zero>()) {
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
	status.set_from<Flag::Overflow>(destination == Numeric::top_bit<IntT>());

	--destination;

	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
	status.set_from<Flag::AuxiliaryCarry>(((destination + 1) ^ destination) & 0x10);
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

	status.set_from<Flag::Overflow, Flag::Carry>(0);
	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
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

	status.set_from<Flag::Overflow, Flag::Carry>(0);
	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
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

	status.set_from<Flag::Overflow, Flag::Carry>(0);
	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
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
	status.set_from<Flag::AuxiliaryCarry>(Numeric::carried_in<4>(IntT(0), destination, IntT(-destination)));

	destination = -destination;

	status.set_from<Flag::Carry>(destination);
	status.set_from<Flag::Overflow>(destination == Numeric::top_bit<IntT>());
	status.set_from<IntT, Flag::Zero, Flag::Sign, Flag::ParityOdd>(destination);
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
void call_relative(IntT offset, RegistersT &registers, FlowControllerT &flow_controller) {
	flow_controller.call(registers.ip() + offset);
}

template <typename IntT, typename FlowControllerT>
void call_absolute(IntT target, FlowControllerT &flow_controller) {
	flow_controller.call(target);
}

template <typename IntT, typename FlowControllerT>
void jump_absolute(IntT target, FlowControllerT &flow_controller) {
	flow_controller.jump(target);
}

template <Model model, typename InstructionT, typename FlowControllerT, typename RegistersT, typename MemoryT>
void call_far(InstructionT &instruction,
	FlowControllerT &flow_controller,
	RegistersT &registers,
	MemoryT &memory
) {
	// TODO: eliminate 16-bit assumption below.
	uint16_t source_address = 0;
	const auto pointer = instruction.destination();
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

template <Model model, typename InstructionT, typename FlowControllerT, typename RegistersT, typename MemoryT>
void jump_far(InstructionT &instruction,
	FlowControllerT &flow_controller,
	RegistersT &registers,
	MemoryT &memory
) {
	// TODO: eliminate 16-bit assumption below.
	uint16_t source_address = 0;
	const auto pointer = instruction.destination();
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
	flow_controller.jump(segment, offset);
}

template <typename FlowControllerT, typename RegistersT, typename MemoryT>
void iret(RegistersT &registers, FlowControllerT &flow_controller, MemoryT &memory, Status &status) {
	// TODO: all modes other than 16-bit real mode.
	registers.ip() = pop<uint16_t>(memory, registers);
	registers.cs() = pop<uint16_t>(memory, registers);
	status.set(pop<uint16_t>(memory, registers));
	flow_controller.did_iret();
}

template <typename InstructionT, typename FlowControllerT, typename RegistersT, typename MemoryT>
void ret_near(InstructionT instruction, RegistersT &registers, FlowControllerT &flow_controller, MemoryT &memory) {
	registers.ip() = pop<uint16_t>(memory, registers);
	registers.sp() += instruction.operand();
	flow_controller.did_near_ret();
}

template <typename InstructionT, typename FlowControllerT, typename RegistersT, typename MemoryT>
void ret_far(InstructionT instruction, RegistersT &registers, FlowControllerT &flow_controller, MemoryT &memory) {
	registers.ip() = pop<uint16_t>(memory, registers);
	registers.cs() = pop<uint16_t>(memory, registers);
	registers.sp() += instruction.operand();
	flow_controller.did_far_ret();
}

template <Model model, Source selector, typename InstructionT, typename MemoryT, typename RegistersT>
void ld(
	InstructionT &instruction,
	uint16_t &destination,
	MemoryT &memory,
	RegistersT &registers
) {
	const auto pointer = instruction.source();
	auto source_address = address<model, uint16_t>(instruction, pointer, registers, memory);
	const Source source_segment = pointer.segment(instruction.segment_override());

	destination = memory.template access<uint16_t>(source_segment, source_address);
	source_address += 2;
	switch(selector) {
		case Source::DS:	registers.ds() = memory.template access<uint16_t>(source_segment, source_address);	break;
		case Source::ES:	registers.es() = memory.template access<uint16_t>(source_segment, source_address);	break;
	}
}

template <Model model, typename IntT, typename InstructionT, typename MemoryT, typename RegistersT>
void lea(
	const InstructionT &instruction,
	IntT &destination,
	MemoryT &memory,
	RegistersT &registers
) {
	// TODO: address size.
	destination = IntT(address<model, uint16_t>(instruction, instruction.source(), registers, memory));
}

template <typename AddressT, typename InstructionT, typename MemoryT, typename RegistersT>
void xlat(
	const InstructionT &instruction,
	MemoryT &memory,
	RegistersT &registers
) {
	Source source_segment = instruction.segment_override();
	if(source_segment == Source::None) source_segment = Source::DS;

	AddressT address;
	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		address = registers.bx() + registers.al();
	}

	registers.al() = memory.template access<uint8_t>(source_segment, address);
}

template <typename IntT>
void mov(IntT &destination, IntT source) {
	destination = source;
}

template <typename FlowControllerT>
void int_(uint8_t vector, FlowControllerT &flow_controller) {
	flow_controller.interrupt(vector);
}

template <typename FlowControllerT>
void into(Status &status, FlowControllerT &flow_controller) {
	if(status.flag<Flag::Overflow>()) {
		flow_controller.interrupt(Interrupt::OnOverflow);
	}
}

inline void sahf(uint8_t &ah, Status &status) {
	/*
		EFLAGS(SF:ZF:0:AF:0:PF:1:CF) ← AH;
	*/
	status.set_from<uint8_t, Flag::Sign>(ah);
	status.set_from<Flag::Zero>(!(ah & 0x40));
	status.set_from<Flag::AuxiliaryCarry>(ah & 0x10);
	status.set_from<Flag::ParityOdd>(!(ah & 0x04));
	status.set_from<Flag::Carry>(ah & 0x01);
}

inline void lahf(uint8_t &ah, Status &status) {
	/*
		AH ← EFLAGS(SF:ZF:0:AF:0:PF:1:CF);
	*/
	ah =
		(status.flag<Flag::Sign>() ? 0x80 : 0x00)	|
		(status.flag<Flag::Zero>() ? 0x40 : 0x00)	|
		(status.flag<Flag::AuxiliaryCarry>() ? 0x10 : 0x00)	|
		(status.flag<Flag::ParityOdd>() ? 0x00 : 0x04)	|
		0x02 |
		(status.flag<Flag::Carry>() ? 0x01 : 0x00);
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
	dx = ax & Numeric::top_bit<IntT>() ? IntT(~0) : IntT(0);
}

// TODO: changes to the interrupt flag do quite a lot more in protected mode.
inline void clc(Status &status) {	status.set_from<Flag::Carry>(0);							}
inline void cld(Status &status) {	status.set_from<Flag::Direction>(0);						}
inline void cli(Status &status) {	status.set_from<Flag::Interrupt>(0);						}
inline void stc(Status &status) {	status.set_from<Flag::Carry>(1);							}
inline void std(Status &status) {	status.set_from<Flag::Direction>(1);						}
inline void sti(Status &status) {	status.set_from<Flag::Interrupt>(1);						}
inline void cmc(Status &status) {	status.set_from<Flag::Carry>(!status.flag<Flag::Carry>());	}

inline void salc(uint8_t &al, const Status &status) {
	al = status.flag<Flag::Carry>() ? 0xff : 0x00;
}

template <typename IntT>
void setmo(IntT &destination, Status &status) {
	destination = ~0;
	status.set_from<Flag::Carry, Flag::AuxiliaryCarry, Flag::Overflow>(0);
	status.set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

template <typename IntT>
void setmoc(IntT &destination, uint8_t cl, Status &status) {
	if(cl) setmo(destination, status);
}

template <typename IntT>
inline void rcl(IntT &destination, uint8_t count, Status &status) {
	/*
		(* RCL and RCR instructions *)
		SIZE ← OperandSize
		CASE (determine count) OF
			SIZE = 8:	tempCOUNT ← (COUNT AND 1FH) MOD 9;
			SIZE = 16:	tempCOUNT ← (COUNT AND 1FH) MOD 17;
			SIZE = 32:	tempCOUNT ← COUNT AND 1FH;
		ESAC;
	*/
	/*
		(* RCL instruction operation *)
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← MSB(DEST);
				DEST ← (DEST * 2) + CF;
				CF ← tempCF;
				tempCOUNT ← tempCOUNT – 1;
			OD;
		ELIHW;
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR CF;
			ELSE OF is undefined;
		FI;
	*/
	/*
		The CF flag contains the value of the bit shifted into it.
		The OF flag is affected only for single- bit rotates (see “Description” above);
		it is undefined for multi-bit rotates. The SF, ZF, AF, and PF flags are not affected.
	*/
	const auto temp_count = count % (Numeric::bit_size<IntT>() + 1);
	auto carry = status.carry_bit<IntT>();
	switch(temp_count) {
		case 0: break;
		case Numeric::bit_size<IntT>(): {
			const IntT temp_carry = destination & 1;
			destination = (destination >> 1) | (carry << (Numeric::bit_size<IntT>() - 1));
			carry = temp_carry;
		} break;
		default: {
			const IntT temp_carry = destination & (Numeric::top_bit<IntT>() >> (temp_count - 1));
			destination =
				(destination << temp_count) |
				(destination >> (Numeric::bit_size<IntT>() + 1 - temp_count)) |
				(carry << (temp_count - 1));
			carry = temp_carry ? 1 : 0;
		} break;
	}

	status.set_from<Flag::Carry>(carry);
	status.set_from<Flag::Overflow>(
		((destination >> (Numeric::bit_size<IntT>() - 1)) & 1) ^ carry
	);
}

template <typename IntT>
inline void rcr(IntT &destination, uint8_t count, Status &status) {
	/*
		(* RCR instruction operation *)
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR CF;
			ELSE OF is undefined;
		FI;
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← LSB(SRC);
				DEST ← (DEST / 2) + (CF * 2SIZE);
				CF ← tempCF;
				tempCOUNT ← tempCOUNT – 1;
			OD;
	*/
	auto carry = status.carry_bit<IntT>();
	status.set_from<Flag::Overflow>(
		((destination >> (Numeric::bit_size<IntT>() - 1)) & 1) ^ carry
	);

	const auto temp_count = count % (Numeric::bit_size<IntT>() + 1);
	switch(temp_count) {
		case 0: break;
		case Numeric::bit_size<IntT>(): {
			const IntT temp_carry = destination & Numeric::top_bit<IntT>();
			destination = (destination << 1) | carry;
			carry = temp_carry;
		} break;
		default: {
			const IntT temp_carry = destination & (1 << (temp_count - 1));
			destination =
				(destination >> temp_count) |
				(destination << (Numeric::bit_size<IntT>() + 1 - temp_count)) |
				(carry << (Numeric::bit_size<IntT>() - temp_count));
			carry = temp_carry;
		} break;
	}

	status.set_from<Flag::Carry>(carry);
}

template <typename IntT>
inline void rol(IntT &destination, uint8_t count, Status &status) {
	/*
		(* ROL and ROR instructions *)
		SIZE ← OperandSize
		CASE (determine count) OF
			SIZE = 8:	tempCOUNT ← COUNT MOD 8;
			SIZE = 16:	tempCOUNT ← COUNT MOD 16;
			SIZE = 32:	tempCOUNT ← COUNT MOD 32;
		ESAC;
	*/
	/*
		(* ROL instruction operation *)
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← MSB(DEST);
				DEST ← (DEST * 2) + tempCF;
				tempCOUNT ← tempCOUNT – 1;
			OD;
		ELIHW;
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR CF;
			ELSE OF is undefined;
		FI;
	*/
	/*
		The CF flag contains the value of the bit shifted into it.
		The OF flag is affected only for single- bit rotates (see “Description” above);
		it is undefined for multi-bit rotates. The SF, ZF, AF, and PF flags are not affected.
	*/
	const auto temp_count = count & (Numeric::bit_size<IntT>() - 1);
	if(!count) {
		// TODO: is this 8086-specific? i.e. do the other x86s also exit without affecting flags when temp_count = 0?
		return;
	}
	if(temp_count) {
		destination =
			(destination << temp_count) |
			(destination >> (Numeric::bit_size<IntT>() - temp_count));
	}

	status.set_from<Flag::Carry>(destination & 1);
	status.set_from<Flag::Overflow>(
		((destination >> (Numeric::bit_size<IntT>() - 1)) ^ destination) & 1
	);
}

template <typename IntT>
inline void ror(IntT &destination, uint8_t count, Status &status) {
	/*
		(* ROL and ROR instructions *)
		SIZE ← OperandSize
		CASE (determine count) OF
			SIZE = 8:	tempCOUNT ← COUNT MOD 8;
			SIZE = 16:	tempCOUNT ← COUNT MOD 16;
			SIZE = 32:	tempCOUNT ← COUNT MOD 32;
		ESAC;
	*/
	/*
		(* ROR instruction operation *)
		WHILE (tempCOUNT ≠ 0)
			DO
				tempCF ← LSB(DEST);
				DEST ← (DEST / 2) + (tempCF * 2^SIZE);
				tempCOUNT ← tempCOUNT – 1;
			OD;
		ELIHW;
		IF COUNT = 1
			THEN OF ← MSB(DEST) XOR MSB - 1 (DEST);
			ELSE OF is undefined;
		FI;
	*/
	/*
		The CF flag contains the value of the bit shifted into it.
		The OF flag is affected only for single- bit rotates (see “Description” above);
		it is undefined for multi-bit rotates. The SF, ZF, AF, and PF flags are not affected.
	*/
	const auto temp_count = count & (Numeric::bit_size<IntT>() - 1);
	if(!count) {
		// TODO: is this 8086-specific? i.e. do the other x86s also exit without affecting flags when temp_count = 0?
		return;
	}
	if(temp_count) {
		destination =
			(destination >> temp_count) |
			(destination << (Numeric::bit_size<IntT>() - temp_count));
	}

	status.set_from<Flag::Carry>(destination & Numeric::top_bit<IntT>());
	status.set_from<Flag::Overflow>(
		(destination ^ (destination << 1)) & Numeric::top_bit<IntT>()
	);
}

/*
	tempCOUNT ← (COUNT AND 1FH);
	tempDEST ← DEST;
	WHILE (tempCOUNT ≠ 0)
	DO
		IF instruction is SAL or SHL
			THEN
				CF ← MSB(DEST);
			ELSE (* instruction is SAR or SHR *)
				CF ← LSB(DEST);
		FI;
		IF instruction is SAL or SHL
			THEN
				DEST ← DEST ∗ 2;
			ELSE
				IF instruction is SAR
					THEN
						DEST ← DEST / 2 (*Signed divide, rounding toward negative infinity*);
					ELSE (* instruction is SHR *)
						DEST ← DEST / 2 ; (* Unsigned divide *);
				FI;
		FI;
		tempCOUNT ← tempCOUNT – 1;
	OD;
	(* Determine overflow for the various instructions *)
	IF COUNT = 1
		THEN
			IF instruction is SAL or SHL
				THEN
					OF ← MSB(DEST) XOR CF;
				ELSE
					IF instruction is SAR
						THEN
							OF ← 0;
						ELSE (* instruction is SHR *)
							OF ← MSB(tempDEST);
					FI;
			FI;
		ELSE
			IF COUNT = 0
				THEN
					All flags remain unchanged;
				ELSE (* COUNT neither 1 or 0 *)
					OF ← undefined;
			FI;
	FI;
*/
/*
	The CF flag contains the value of the last bit shifted out of the destination operand;
	it is undefined for SHL and SHR instructions where the count is greater than or equal to
	the size (in bits) of the destination operand. The OF flag is affected only for 1-bit shifts
	(see “Description” above); otherwise, it is undefined.

	The SF, ZF, and PF flags are set according to the result. If the count is 0, the flags are not affected.
	For a non-zero count, the AF flag is undefined.
*/
template <typename IntT>
inline void sal(IntT &destination, uint8_t count, Status &status) {
	switch(count) {
		case 0:	return;
		case Numeric::bit_size<IntT>():
			status.set_from<Flag::Carry, Flag::Overflow>(destination & 1);
			destination = 0;
		break;
		default:
			if(count > Numeric::bit_size<IntT>()) {
				status.set_from<Flag::Carry, Flag::Overflow>(0);
				destination = 0;
			} else {
				const auto mask = (Numeric::top_bit<IntT>() >> (count - 1));
				status.set_from<Flag::Carry>(
					 destination & mask
				);
				status.set_from<Flag::Overflow>(
					 (destination ^ (destination << 1)) & mask
				);
				destination <<= count;
			}
		break;
	}
	status.set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

template <typename IntT>
inline void sar(IntT &destination, uint8_t count, Status &status) {
	if(!count) {
		return;
	}

	const IntT sign = Numeric::top_bit<IntT>() & destination;
	if(count >= Numeric::bit_size<IntT>()) {
		destination = sign ? IntT(~0) : IntT(0);
		status.set_from<Flag::Carry>(sign);
	} else {
		const IntT mask = 1 << (count - 1);
		status.set_from<Flag::Carry>(destination & mask);
		destination = (destination >> count) | (sign ? ~(IntT(~0) >> count) : 0);
	}
	status.set_from<Flag::Overflow>(0);
	status.set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

template <typename IntT>
inline void shr(IntT &destination, uint8_t count, Status &status) {
	if(!count) {
		return;
	}

	status.set_from<Flag::Overflow>(Numeric::top_bit<IntT>() & destination);
	if(count == Numeric::bit_size<IntT>()) {
		status.set_from<Flag::Carry>(Numeric::top_bit<IntT>() & destination);
		destination = 0;
	} else if(count > Numeric::bit_size<IntT>()) {
		status.set_from<Flag::Carry>(0);
		destination = 0;
	} else {
		const IntT mask = 1 << (count - 1);
		status.set_from<Flag::Carry>(destination & mask);
		destination >>= count;
	}
	status.set_from<IntT, Flag::Sign, Flag::Zero, Flag::ParityOdd>(destination);
}

template <typename MemoryT, typename RegistersT>
void popf(MemoryT &memory, RegistersT &registers, Status &status) {
	status.set(pop<uint16_t>(memory, registers));
}

template <typename MemoryT, typename RegistersT>
void pushf(MemoryT &memory, RegistersT &registers, Status &status) {
	uint16_t value = status.get();
	push<uint16_t>(value, memory, registers);
}

template <typename AddressT, typename InstructionT, typename RegistersT>
bool repetition_over(const InstructionT &instruction, RegistersT &registers) {
	if(instruction.repetition() == Repetition::None) {
		return false;
	}

	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		return !registers.cx();
	} else {
		return !registers.ecx();
	}
}

template <typename AddressT, typename InstructionT, typename RegistersT, typename FlowControllerT>
void repeat(const InstructionT &instruction, Status &status, RegistersT &registers, FlowControllerT &flow_controller) {
	if(instruction.repetition() == Repetition::None) {
		return;
	}

	bool count_exhausted = false;

	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		count_exhausted = !(--registers.cx());
	} else {
		count_exhausted = !(--registers.ecx());
	}

	if(count_exhausted) {
		return;
	}
	const bool zero = status.flag<Flag::Zero>();
	if(instruction.repetition() == Repetition::RepE && !zero) {
		return;
	} else if(instruction.repetition() == Repetition::RepNE && zero) {
		return;
	}

	flow_controller.repeat_last();
}

template <typename IntT, typename AddressT, typename InstructionT, typename MemoryT, typename RegistersT, typename FlowControllerT>
void cmps(const InstructionT &instruction, MemoryT &memory, RegistersT &registers, Status &status, FlowControllerT &flow_controller) {
	if(repetition_over<AddressT>(instruction, registers)) {
		return;
	}

	Source source_segment = instruction.segment_override();
	if(source_segment == Source::None) source_segment = Source::DS;

	if constexpr (std::is_same_v<AddressT, uint16_t>) {
		IntT source = memory.template access<IntT>(source_segment, registers.si());
		IntT destination = memory.template access<IntT>(Source::ES, registers.di());
		Primitive::sub<false, false>(destination, source, status);
		registers.si() += status.direction<AddressT>();
		registers.di() += status.direction<AddressT>();
	} else {
		IntT source = memory.template access<IntT>(source_segment, registers.esi());
		IntT destination = memory.template access<IntT>(Source::ES, registers.edi());
		Primitive::sub<false, false>(destination, source, status);
		registers.esi() += status.direction<AddressT>();
		registers.edi() += status.direction<AddressT>();
	}

	repeat<AddressT>(instruction, status, registers, flow_controller);
}

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
//	using AddressT = typename AddressT<is_32bit(model)>::type;

	// Establish source() and destination() shorthand to fetch data if necessary.
	IntT immediate;
	const auto source = [&]() -> IntT& {
		return *resolve<model, IntT>(
			instruction,
			instruction.source().template source<false>(),
			instruction.source(),
			registers,
			memory,
			nullptr,
			&immediate);
	};
	const auto destination = [&]() -> IntT& {
		return *resolve<model, IntT>(
			instruction,
			instruction.destination().template source<false>(),
			instruction.destination(),
			registers,
			memory,
			nullptr,
			&immediate);
	};

	// Performs a displacement jump only if @c condition is true.
	const auto jcc = [&](bool condition) {
		Primitive::jump(
			condition,
			instruction.displacement(),
			registers,
			flow_controller);
	};

	const auto shift_count = [&]() -> uint8_t {
		static constexpr uint8_t mask = (model != Model::i8086) ? 0x1f : 0xff;
		switch(instruction.source().template source<false>()) {
			case Source::None:		return 1;
			case Source::Immediate:	return uint8_t(instruction.operand()) & mask;
			default:				return registers.cl() & mask;
		}
	};

	// Some instructions use a pair of registers as an extended accumulator — DX:AX or EDX:EAX.
	// The two following return the high and low parts of that pair; they also work in Byte mode to return AH:AL,
	// i.e. AX split into high and low parts.
	const auto pair_high = [&]() -> IntT& {
		if constexpr (data_size == DataSize::Byte) 			return registers.ah();
		else if constexpr (data_size == DataSize::Word)		return registers.dx();
		else if constexpr (data_size == DataSize::DWord)	return registers.edx();
	};
	const auto pair_low = [&]() -> IntT& {
		if constexpr (data_size == DataSize::Byte) 			return registers.al();
		else if constexpr (data_size == DataSize::Word)		return registers.ax();
		else if constexpr (data_size == DataSize::DWord)	return registers.eax();
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

		case Operation::CBW:	Primitive::cbw(pair_low());					return;
		case Operation::CWD:	Primitive::cwd(pair_high(), pair_low());	return;

		case Operation::ESC:
		case Operation::NOP:	return;

		case Operation::HLT:	flow_controller.halt();		return;
		case Operation::WAIT:	flow_controller.wait();		return;

		case Operation::ADC:	Primitive::add<true>(destination(), source(), status);			break;
		case Operation::ADD:	Primitive::add<false>(destination(), source(), status);			break;
		case Operation::SBB:	Primitive::sub<true, true>(destination(), source(), status);	break;
		case Operation::SUB:	Primitive::sub<false, true>(destination(), source(), status);	break;
		case Operation::CMP:	Primitive::sub<false, false>(destination(), source(), status);	break;
		case Operation::TEST:	Primitive::test(destination(), source(), status);				break;

		case Operation::MUL:	Primitive::mul(pair_high(), pair_low(), source(), status);				return;
		case Operation::IMUL_1:	Primitive::imul(pair_high(), pair_low(), source(), status);				return;
		case Operation::DIV:	Primitive::div(pair_high(), pair_low(), source(), flow_controller);		return;
		case Operation::IDIV:	Primitive::idiv(pair_high(), pair_low(), source(), flow_controller);	return;

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

		case Operation::JMPrel:	jcc(true);																		return;
		case Operation::JMPabs:	Primitive::jump_absolute(destination(), flow_controller);						return;
		case Operation::JMPfar:	Primitive::jump_far<model>(instruction, flow_controller, registers, memory);	return;

		// TODO: use ECX rather than CX for all of below if address size is 32-bit.
		case Operation::JCXZ:	jcc(!registers.cx());								return;
		case Operation::LOOP:	Primitive::loop(registers.cx(), instruction.offset(), registers, flow_controller);				return;
		case Operation::LOOPE:	Primitive::loope(registers.cx(), instruction.offset(), registers, status, flow_controller);		return;
		case Operation::LOOPNE:	Primitive::loopne(registers.cx(), instruction.offset(), registers, status, flow_controller);	return;

		case Operation::IRET:		Primitive::iret(registers, flow_controller, memory, status);			return;
		case Operation::RETnear:	Primitive::ret_near(instruction, registers, flow_controller, memory);	return;
		case Operation::RETfar:		Primitive::ret_far(instruction, registers, flow_controller, memory);	return;

		case Operation::INT:	Primitive::int_(instruction.operand(), flow_controller);	return;
		case Operation::INTO:	Primitive::into(status, flow_controller);					return;

		case Operation::SAHF:	Primitive::sahf(registers.ah(), status);			return;
		case Operation::LAHF:	Primitive::lahf(registers.ah(), status);			return;

		case Operation::LDS:	if constexpr (data_size == DataSize::Word) Primitive::ld<model, Source::DS>(instruction, destination(), memory, registers);	return;
		case Operation::LES:	if constexpr (data_size == DataSize::Word) Primitive::ld<model, Source::ES>(instruction, destination(), memory, registers);	return;

		case Operation::LEA:	Primitive::lea<model>(instruction, destination(), memory, registers);	return;
		case Operation::MOV:	Primitive::mov(destination(), source());								return;

		case Operation::JO:		jcc(status.condition<Condition::Overflow>());		return;
		case Operation::JNO:	jcc(!status.condition<Condition::Overflow>());		return;
		case Operation::JB:		jcc(status.condition<Condition::Below>());			return;
		case Operation::JNB:	jcc(!status.condition<Condition::Below>());			return;
		case Operation::JZ:		jcc(status.condition<Condition::Zero>());			return;
		case Operation::JNZ:	jcc(!status.condition<Condition::Zero>());			return;
		case Operation::JBE:	jcc(status.condition<Condition::BelowOrEqual>());	return;
		case Operation::JNBE:	jcc(!status.condition<Condition::BelowOrEqual>());	return;
		case Operation::JS:		jcc(status.condition<Condition::Sign>());			return;
		case Operation::JNS:	jcc(!status.condition<Condition::Sign>());			return;
		case Operation::JP:		jcc(!status.condition<Condition::ParityOdd>());		return;
		case Operation::JNP:	jcc(status.condition<Condition::ParityOdd>());		return;
		case Operation::JL:		jcc(status.condition<Condition::Less>());			return;
		case Operation::JNL:	jcc(!status.condition<Condition::Less>());			return;
		case Operation::JLE:	jcc(status.condition<Condition::LessOrEqual>());	return;
		case Operation::JNLE:	jcc(!status.condition<Condition::LessOrEqual>());	return;

		case Operation::RCL:	Primitive::rcl(destination(), shift_count(), status);	break;
		case Operation::RCR:	Primitive::rcr(destination(), shift_count(), status);	break;
		case Operation::ROL:	Primitive::rol(destination(), shift_count(), status);	break;
		case Operation::ROR:	Primitive::ror(destination(), shift_count(), status);	break;
		case Operation::SAL:	Primitive::sal(destination(), shift_count(), status);	break;
		case Operation::SAR:	Primitive::sar(destination(), shift_count(), status);	break;
		case Operation::SHR:	Primitive::shr(destination(), shift_count(), status);	break;

		case Operation::CLC:	Primitive::clc(status);				return;
		case Operation::CLD:	Primitive::cld(status);				return;
		case Operation::CLI:	Primitive::cli(status);				return;
		case Operation::STC:	Primitive::stc(status);				return;
		case Operation::STD:	Primitive::std(status);				return;
		case Operation::STI:	Primitive::sti(status);				return;
		case Operation::CMC:	Primitive::cmc(status);				return;

		case Operation::XCHG:	Primitive::xchg(destination(), source());	return;

		case Operation::SALC:	Primitive::salc(registers.al(), status);					return;
		case Operation::SETMO:
			if constexpr (model == Model::i8086) {
				Primitive::setmo(destination(), status);
			} else {
				// TODO.
			}
		return;
		case Operation::SETMOC:
			if constexpr (model == Model::i8086) {
				Primitive::setmoc(destination(), registers.cl(), status);
			} else {
				// TODO.
			}
		return;

		case Operation::XLAT:	Primitive::xlat<uint16_t>(instruction, memory, registers);	return;

		case Operation::POP:	source() = Primitive::pop<IntT>(memory, registers);		break;
		case Operation::PUSH:	Primitive::push<IntT>(source(), memory, registers);		break;
		case Operation::POPF:	Primitive::popf(memory, registers, status);				break;
		case Operation::PUSHF:	Primitive::pushf(memory, registers, status);			break;

		// TODO: don't assume address size below.
		case Operation::CMPS:
			Primitive::cmps<IntT, uint16_t>(instruction, memory, registers, status, flow_controller);
		break;
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

	// TODO: incorporate and propagate address size.

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
			[[fallthrough]];
		case DataSize::None:
			assert(false);
		break;
	}
}

}

#endif /* PerformImplementation_h */
