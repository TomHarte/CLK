//
//  DataPointerResolver.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/02/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef DataPointerResolver_hpp
#define DataPointerResolver_hpp

#include "Instruction.hpp"
#include "Model.hpp"

#include <cassert>

namespace InstructionSet {
namespace x86 {

/// Unlike source, describes only registers, and breaks
/// them down by conventional name — so AL, AH, AX and EAX are all
/// listed separately and uniquely, rather than being eAX+size or
/// eSPorAH with a size of 1.
enum class Register: uint8_t {
	// 8-bit registers.
	AL, AH,
	CL, CH,
	DL, DH,
	BL, BH,

	// 16-bit registers.
	AX, CX, DX, BX,
	SP, BP, SI, DI,
	ES, CS, SS, DS,
	FS, GS,

	// 32-bit registers.
	EAX, ECX, EDX, EBX,
	ESP, EBP, ESI, EDI,

	//
	None
};

/// @returns @c true if @c r is the same size as @c DataT; @c false otherwise.
/// @discussion Provided primarily to aid in asserts; if the decoder and resolver are both
/// working then it shouldn't be necessary to test this in register files.
template <typename DataT> constexpr bool is_sized(Register r) {
	static_assert(sizeof(DataT) == 4 || sizeof(DataT) == 2 || sizeof(DataT) == 1);

	if constexpr (sizeof(DataT) == 4) {
		return r >= Register::EAX && r < Register::None;
	}

	if constexpr (sizeof(DataT) == 2) {
		return r >= Register::AX && r < Register::EAX;
	}

	if constexpr (sizeof(DataT) == 1) {
		return r >= Register::AL && r < Register::AX;
	}

	return false;
}

/// @returns the proper @c Register given @c source and data of size @c sizeof(DataT),
/// or Register::None if no such register exists (e.g. asking for a 32-bit version of CS).
template <typename DataT> constexpr Register register_for_source(Source source) {
	static_assert(sizeof(DataT) == 4 || sizeof(DataT) == 2 || sizeof(DataT) == 1);

	if constexpr (sizeof(DataT) == 4) {
		switch(source) {
			case Source::eAX:		return Register::EAX;
			case Source::eCX:		return Register::ECX;
			case Source::eDX:		return Register::EDX;
			case Source::eBX:		return Register::EBX;
			case Source::eSPorAH:	return Register::ESP;
			case Source::eBPorCH:	return Register::EBP;
			case Source::eSIorDH:	return Register::ESI;
			case Source::eDIorBH:	return Register::EDI;

			default:				break;
		}
	}

	if constexpr (sizeof(DataT) == 2) {
		switch(source) {
			case Source::eAX:		return Register::AX;
			case Source::eCX:		return Register::CX;
			case Source::eDX:		return Register::DX;
			case Source::eBX:		return Register::BX;
			case Source::eSPorAH:	return Register::SP;
			case Source::eBPorCH:	return Register::BP;
			case Source::eSIorDH:	return Register::SI;
			case Source::eDIorBH:	return Register::DI;
			case Source::ES:		return Register::ES;
			case Source::CS:		return Register::CS;
			case Source::SS:		return Register::SS;
			case Source::DS:		return Register::DS;
			case Source::FS:		return Register::FS;
			case Source::GS:		return Register::GS;

			default:				break;
		}
	}

	if constexpr (sizeof(DataT) == 1) {
		switch(source) {
			case Source::eAX:		return Register::AL;
			case Source::eCX:		return Register::CL;
			case Source::eDX:		return Register::DL;
			case Source::eBX:		return Register::BL;
			case Source::eSPorAH:	return Register::AH;
			case Source::eBPorCH:	return Register::CH;
			case Source::eSIorDH:	return Register::DH;
			case Source::eDIorBH:	return Register::BH;

			default:				break;
		}
	}

	return Register::None;
}

/// Reads from or writes to the source or target identified by a DataPointer, relying upon two user-supplied classes:
///
/// * a register bank; and
/// * a memory pool.
///
/// The register bank should implement `template<typename DataT, Register> DataT read()` and `template<typename DataT, Register> void write(DataT)`.
/// Those functions will be called only with registers and data types that are appropriate to the @c model.
///
/// The memory pool should implement `template<typename DataT> DataT read(Source segment, uint32_t address)` and
/// `template<typename DataT> void write(Source segment, uint32_t address, DataT value)`.
template <Model model, typename RegistersT, typename MemoryT> class DataPointerResolver {
	public:
	public:
		/// Reads the data pointed to by @c pointer, referencing @c instruction, @c memory and @c registers as necessary.
		template <typename DataT> static DataT read(
			RegistersT &registers,
			MemoryT &memory,
			const Instruction<is_32bit(model)> &instruction,
			DataPointer pointer);

		/// Writes @c value to the data pointed to by @c pointer, referencing @c instruction, @c memory and @c registers as necessary.
		template <typename DataT> static void write(
			RegistersT &registers,
			MemoryT &memory,
			const Instruction<is_32bit(model)> &instruction,
			DataPointer pointer,
			DataT value);

		/// Computes the effective address of @c pointer including any displacement applied by @c instruction.
		/// @c pointer must be of type Source::Indirect.
		static uint32_t effective_address(
			RegistersT &registers,
			const Instruction<is_32bit(model)> &instruction,
			DataPointer pointer);

	private:
		template <bool is_write, typename DataT> static void access(
			RegistersT &registers,
			MemoryT &memory,
			const Instruction<is_32bit(model)> &instruction,
			DataPointer pointer,
			DataT &value);
};


//
//	Implementation begins here.
//

template <Model model, typename RegistersT, typename MemoryT>
template <typename DataT> DataT DataPointerResolver<model, RegistersT, MemoryT>::read(
	RegistersT &registers,
	MemoryT &memory,
	const Instruction<is_32bit(model)> &instruction,
	DataPointer pointer) {
		DataT result;
		access<false>(registers, memory, instruction, pointer, result);
		return result;
	}

template <Model model, typename RegistersT, typename MemoryT>
template <typename DataT> void DataPointerResolver<model, RegistersT, MemoryT>::write(
	RegistersT &registers,
	MemoryT &memory,
	const Instruction<is_32bit(model)> &instruction,
	DataPointer pointer,
	DataT value) {
		access<true>(registers, memory, instruction, pointer, value);
	}

#define rw(v, r, is_write)														\
	case Source::r:																\
		using VType = typename std::remove_reference<decltype(v)>::type;		\
		if constexpr (is_write) {												\
			registers.template write<VType, register_for_source<VType>(Source::r)>(v);		\
		} else {																\
			 v = registers.template read<VType, register_for_source<VType>(Source::r)>();	\
		}																		\
	break;

#define ALLREGS(v, i)	rw(v, eAX, i); 		rw(v, eCX, i); 		\
						rw(v, eDX, i);		rw(v, eBX, i); 		\
						rw(v, eSPorAH, i);	rw(v, eBPorCH, i);	\
						rw(v, eSIorDH, i);	rw(v, eDIorBH, i);	\
						rw(v, ES, i);		rw(v, CS, i); 		\
						rw(v, SS, i);		rw(v, DS, i); 		\
						rw(v, FS, i);		rw(v, GS, i);

template <Model model, typename RegistersT, typename MemoryT>
uint32_t DataPointerResolver<model, RegistersT, MemoryT>::effective_address(
	RegistersT &registers,
	const Instruction<is_32bit(model)> &instruction,
	DataPointer pointer) {
		using AddressT = typename Instruction<is_32bit(model)>::AddressT;
		AddressT base = 0, index = 0;

		switch(pointer.base()) {
			default: break;
			ALLREGS(base, false);
		}

		switch(pointer.index()) {
			default: break;
			ALLREGS(index, false);
		}

		uint32_t address = index;
		if constexpr (model >= Model::i80386) {
			address <<= pointer.scale();
		} else {
			assert(!pointer.scale());
		}

		// Always compute address as 32-bit.
		// TODO: verify use of memory_mask around here.
		// Also I think possibly an exception is supposed to be generated
		// if the programmer is in 32-bit mode and has asked for 16-bit
		// address computation but generated e.g. a 17-bit result. Look into
		// that when working on execution. For now the goal is merely decoding
		// and this code exists both to verify the presence of all necessary
		// fields and to help to explore the best breakdown of storage
		// within Instruction.
		constexpr uint32_t memory_masks[] = {0x0000'ffff, 0xffff'ffff};
		const uint32_t memory_mask = memory_masks[int(instruction.address_size())];
		address = (address & memory_mask) + (base & memory_mask) + instruction.displacement();
		return address;
	}

template <Model model, typename RegistersT, typename MemoryT>
template <bool is_write, typename DataT> void DataPointerResolver<model, RegistersT, MemoryT>::access(
	RegistersT &registers,
	MemoryT &memory,
	const Instruction<is_32bit(model)> &instruction,
	DataPointer pointer,
	DataT &value) {
		const Source source = pointer.source();

		switch(source) {
			default:
				if constexpr (!is_write) {
					value = 0;
				}
			return;

			ALLREGS(value, is_write);

			case Source::DirectAddress:
				if constexpr(is_write) {
					memory.template write(instruction.data_segment(), instruction.displacement(), value);
				} else {
					value = memory.template read<DataT>(instruction.data_segment(), instruction.displacement());
				}
			break;
			case Source::Immediate:
				value = DataT(instruction.operand());
			break;

			case Source::Indirect: {
				const auto address = effective_address(registers, instruction, pointer);

				if constexpr (is_write) {
					memory.template write(
						instruction.data_segment(),
						address,
						value
					);
				} else {
					value = memory.template read<DataT>(
						instruction.data_segment(),
						address
					);
				}
			}
		}
	}
#undef ALLREGS
#undef read_or_write

}
}

#endif /* DataPointerResolver_hpp */
