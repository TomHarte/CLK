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

void aaa(CPU::RegisterPair16 &ax, Status &status) {
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
}

void aad(CPU::RegisterPair16 &ax, uint8_t imm, Status &status) {
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

void aam(CPU::RegisterPair16 &ax, uint8_t imm, Status &status) {
	/*
		tempAL ← AL;
		AH ← tempAL / imm8; (* imm8 is set to 0AH for the AAD mnemonic *)
		AL ← tempAL MOD imm8;
	*/
	/*
		The SF, ZF, and PF flags are set according to the result.
		The OF, AF, and CF flags are undefined.
	*/
	ax.halves.high = ax.halves.low / imm;
	ax.halves.low = ax.halves.low % imm;
	status.sign = ax.halves.low & 0x80;
	status.parity = status.zero = ax.halves.low;
}

void aas(CPU::RegisterPair16 &ax, Status &status) {
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
	status.sign = status.zero = status.parity = result;
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
	status.sign = status.zero = status.parity = result;
	status.overflow = overflow<true, IntT>(destination, source, result);

	destination = result;
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
	[[maybe_unused]] FlowControllerT &flow_controller,
	RegistersT &registers,
	[[maybe_unused]] MemoryT &memory,
	[[maybe_unused]] IOT &io
) {
	using IntT = typename DataSizeType<data_size>::type;
	using AddressT = typename AddressT<is_32bit(model)>::type;

	// Establish source() and destination() shorthand to fetch data if necessary.
	IntT fetched_data = 0, original_data = 0;
	Source segment;
	AddressT address;

	static constexpr IntT zero = 0;
	auto data = [&](DataPointer source) -> IntT& {
		// Rules:
		//
		// * if this is a memory access, set target_address and break;
		// * otherwise return the appropriate value.
		switch(source.source<false>()) {
			case Source::eAX:
				switch(data_size) {
					default:				return registers.al();
					case DataSize::Word:	return registers.ax();
					case DataSize::DWord:	return registers.eax();
				}
			case Source::eCX:
				switch(data_size) {
					default:				return registers.cl();
					case DataSize::Word:	return registers.cx();
					case DataSize::DWord:	return registers.ecx();
				}
			case Source::eDX:
				switch(data_size) {
					default:				return registers.dl();
					case DataSize::Word:	return registers.dx();
					case DataSize::DWord:	return registers.edx();
				}
			case Source::eBX:
				switch(data_size) {
					default:				return registers.bl();
					case DataSize::Word:	return registers.bx();
					case DataSize::DWord:	return registers.ebx();
				}
			case Source::eSPorAH:
				switch(data_size) {
					default:				return registers.ah();
					case DataSize::Word:	return registers.sp();
					case DataSize::DWord:	return registers.esp();
				}
			case Source::eBPorCH:
				switch(data_size) {
					default:				return registers.ch();
					case DataSize::Word:	return registers.bp();
					case DataSize::DWord:	return registers.ebp();
				}
			case Source::eSIorDH:
				switch(data_size) {
					default:				return registers.dh();
					case DataSize::Word:	return registers.si();
					case DataSize::DWord:	return registers.esi();
				}
			case Source::eDIorBH:
				switch(data_size) {
					default:				return registers.bh();
					case DataSize::Word:	return registers.di();
					case DataSize::DWord:	return registers.edi();
				}

			case Source::ES:	return registers.es();
			case Source::CS:	return registers.cs();
			case Source::SS:	return registers.ss();
			case Source::DS:	return registers.ds();
			case Source::FS:	return registers.fs();
			case Source::GS:	return registers.gs();

			case Source::Immediate:			// TODO (here the use of a reference falls down?)

			case Source::None:		return zero;

			case Source::Indirect:			// TODO
			case Source::IndirectNoBase:	// TODO

			case Source::DirectAddress:
				address = instruction.offset();
			break;
		}

		// If execution has reached here then a memory fetch is required.
		// Do it and exit.
		segment = Source::DS;	// TODO.
		fetched_data = original_data = memory.template read<IntT>(segment, address);
		return fetched_data;
	};

	auto source = [&]() -> IntT& 		{	return data(instruction.source());		};
	auto destination = [&]() -> IntT& 	{	return data(instruction.destination());	};

	// Guide to the below:
	//
	//	* use hard-coded register names where appropriate;
	//	* return directly if there is definitely no possible write back to RAM;
	//	* otherwise use the source() and destination() lambdas, and break in order to allow a writeback if necessary.
	switch(instruction.operation) {
		case Operation::AAA:	Primitive::aaa(registers.ax(), status);								return;
		case Operation::AAD:	Primitive::aad(registers.ax(), instruction.immediate(), status);	return;
		case Operation::AAM:	Primitive::aam(registers.ax(), instruction.immediate(), status);	return;
		case Operation::AAS:	Primitive::aas(registers.ax(), status);								return;

		case Operation::ADC:	Primitive::adc(destination(), source(), status);					break;
		case Operation::ADD:	Primitive::add(destination(), source(), status);					break;
	}

	// Write to memory if required to complete this operation.
	if(original_data != fetched_data) {
		// TODO.
	}
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
