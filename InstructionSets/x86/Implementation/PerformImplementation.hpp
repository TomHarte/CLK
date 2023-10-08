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

namespace InstructionSet::x86 {

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
	const IntT output_changed = result ^ rhs;
	const IntT input_differed = lhs ^ rhs;

	if constexpr (is_add) {
		return top_bit<IntT>() & output_changed & ~input_differed;
	} else {
		return top_bit<IntT>() & output_changed & input_differed;
	}
}

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
		flow_controller.interrupt(Interrupt::DivideByZero);
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

template <typename IntT>
void adc(IntT &destination, IntT source, Status &status) {
	/*
		DEST ← DEST + SRC + CF;
	*/
	/*
		The OF, SF, ZF, AF, CF, and PF flags are set according to the result.
	*/
	const IntT result = destination + source + status.carry_bit<IntT>();

	status.carry = Numeric::carried_out<bit_size<IntT>() - 1>(destination, source, result);
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

	status.carry = Numeric::carried_out<bit_size<IntT>() - 1>(destination, source, result);
	status.auxiliary_carry = Numeric::carried_in<4>(destination, source, result);
	status.sign = result & top_bit<IntT>();
	status.zero = status.parity = result;
	status.overflow = overflow<true, IntT>(destination, source, result);

	destination = result;
}

}

template <Model model, DataSize data_size, typename InstructionT, typename RegistersT, typename MemoryT>
typename DataSizeType<data_size>::type *resolve(InstructionT &instruction, Source source, DataPointer pointer, RegistersT &registers, MemoryT &memory) {
	// Rules:
	//
	// * if this is a memory access, set target_address and break;
	// * otherwise return the appropriate value.
	uint32_t address;
	switch(source) {
		case Source::eAX:
			// Slightly contorted if chain here and below:
			//
			//	(i) does the `constexpr` version of a `switch`; and
			//	(i) ensures .eax() etc aren't called on @c registers for 16-bit processors, so they need not implement 32-bit storage.
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.eax();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.ax();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.al();		}
			else 															{	return nullptr;				}
		case Source::eCX:
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.ecx();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.cx();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.cl();		}
			else 															{	return nullptr;				}
		case Source::eDX:
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.edx();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.dx();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.dl();		}
			else if constexpr (data_size == DataSize::DWord)				{	return nullptr;				}
		case Source::eBX:
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.ebx();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.bx();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.bl();		}
			else if constexpr (data_size == DataSize::DWord)				{	return nullptr;				}
		case Source::eSPorAH:
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.esp();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.sp();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.ah();		}
			else															{	return nullptr;				}
		case Source::eBPorCH:
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.ebp();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.bp();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.ch();		}
			else 															{	return nullptr;				}
		case Source::eSIorDH:
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.esi();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.si();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.dh();		}
			else 															{	return nullptr;				}
		case Source::eDIorBH:
			if constexpr (is_32bit(model) && data_size == DataSize::DWord) 	{	return &registers.edi();	}
			else if constexpr (data_size == DataSize::Word)					{	return &registers.di();		}
			else if constexpr (data_size == DataSize::Byte)					{	return &registers.bh();		}
			else															{	return nullptr;				}

		case Source::ES:	if constexpr (data_size == DataSize::Word) return &registers.es(); else return nullptr;
		case Source::CS:	if constexpr (data_size == DataSize::Word) return &registers.cs(); else return nullptr;
		case Source::SS:	if constexpr (data_size == DataSize::Word) return &registers.ss(); else return nullptr;
		case Source::DS:	if constexpr (data_size == DataSize::Word) return &registers.ds(); else return nullptr;

		// 16-bit models don't have FS and GS.
		case Source::FS:	if constexpr (is_32bit(model) && data_size == DataSize::Word) return &registers.fs(); else return nullptr;
		case Source::GS:	if constexpr (is_32bit(model) && data_size == DataSize::Word) return &registers.gs(); else return nullptr;

		case Source::Immediate:			// TODO (here the use of a pointer falls down?)

		case Source::None:		return nullptr;

		case Source::Indirect:			// TODO: non-word indexes and bases.
			address = *resolve<model, DataSize::Word>(instruction, pointer.index(), pointer, registers, memory);
			if constexpr (is_32bit(model)) {
				address <<= pointer.scale();
			}
			address += instruction.offset() + *resolve<model, DataSize::Word>(instruction, pointer.base(), pointer, registers, memory);
		break;

		case Source::IndirectNoBase:	// TODO: non-word indexes and bases.
			address = *resolve<model, DataSize::Word>(instruction, pointer.index(), pointer, registers, memory);
			if constexpr (is_32bit(model)) {
				address <<= pointer.scale();
			}
			address += instruction.offset();
		break;

		case Source::DirectAddress:
			address = instruction.offset();
		break;
	}

	// If execution has reached here then a memory fetch is required.
	// Do it and exit.
	const Source segment = pointer.segment(instruction.segment_override());
	using IntT = typename DataSizeType<data_size>::type;
	return &memory.template access<IntT>(segment, address);
};

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
	[[maybe_unused]] FlowControllerT &flow_controller,
	RegistersT &registers,
	[[maybe_unused]] MemoryT &memory,
	[[maybe_unused]] IOT &io
) {
	using IntT = typename DataSizeType<data_size>::type;
	using AddressT = typename AddressT<is_32bit(model)>::type;

	// Establish source() and destination() shorthand to fetch data if necessary.
	auto source = [&]() -> IntT& 		{	return *resolve<model, data_size>(instruction, instruction.source().template source<false>(), instruction.source(), registers, memory);			};
	auto destination = [&]() -> IntT& 	{	return *resolve<model, data_size>(instruction, instruction.destination().template source<false>(), instruction.source(), registers, memory);	};

	// Guide to the below:
	//
	//	* use hard-coded register names where appropriate;
	//	* return directly if there is definitely no possible write back to RAM;
	//	* otherwise use the source() and destination() lambdas, and break in order to allow a writeback if necessary.
	switch(instruction.operation) {
		default: return;
		//assert(false);

		case Operation::AAA:	Primitive::aaa(registers.axp(), status);											return;
		case Operation::AAD:	Primitive::aad(registers.axp(), instruction.operand(), status);						return;
		case Operation::AAM:	Primitive::aam(registers.axp(), instruction.operand(), status, flow_controller);	return;
		case Operation::AAS:	Primitive::aas(registers.axp(), status);											return;

		case Operation::ADC:	Primitive::adc(destination(), source(), status);									break;
		case Operation::ADD:	Primitive::add(destination(), source(), status);									break;
	}

	// Write to memory if required to complete this operation.
//	if(original_data != fetched_data) {
		// TODO.
//	}
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
			perform<model, DataSize::DWord>(instruction, status, flow_controller, registers, memory, io);
		break;
		case DataSize::None:
			perform<model, DataSize::None>(instruction, status, flow_controller, registers, memory, io);
		break;
	}
}


}

#endif /* PerformImplementation_h */
