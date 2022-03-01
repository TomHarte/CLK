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
	AL, AH, AX, EAX,
	CL, CH, CX, ECX,
	DL, DH, DX, EDX,
	BL, BH, BX, EBX,
	SP, ESP,
	BP, EBP,
	SI, ESI,
	DI, EDI,
	ES,
	CS,
	SS,
	DS,
	FS,
	GS,
	None
};

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
		access<true>(registers, memory, instruction, pointer, result);
		return result;
	}

template <Model model, typename RegistersT, typename MemoryT>
template <typename DataT> void DataPointerResolver<model, RegistersT, MemoryT>::write(
	RegistersT &registers,
	MemoryT &memory,
	const Instruction<is_32bit(model)> &instruction,
	DataPointer pointer,
	DataT value) {
		access<false>(registers, memory, instruction, pointer, value);
	}

#define rw(v, r, is_write)														\
	case Source::r: {															\
		if constexpr (is_write) {												\
			registers.template write<decltype(v), register_for_source<decltype(v)>(Source::r)>(v);						\
		} else {																\
			v = registers.template read<decltype(v), register_for_source<decltype(v)>(Source::r)>();					\
		}																		\
	} break;

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

		// Always compute address as 32-bit.
		// TODO: verify application of memory_mask around here.
		// The point of memory_mask is that 32-bit x86 offers the memory size modifier,
		// permitting 16-bit addresses to be generated in 32-bit mode and vice versa.
		// To figure out is at what point in the calculation the 16-bit constraint is
		// applied when active.
		uint32_t address = index;
		if constexpr (model >= Model::i80386) {
			address <<= pointer.scale();
		} else {
			assert(!pointer.scale());
		}

		constexpr uint32_t memory_masks[] = {0x0000'ffff, 0xffff'ffff};
		const uint32_t memory_mask = memory_masks[instruction.address_size_is_32()];
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
					memory.template write<DataT>(instruction.data_segment(), instruction.displacement(), value);
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
					value = memory.template read<DataT>(
						instruction.data_segment(),
						address
					);
				} else {
					memory.template write<DataT>(
						instruction.data_segment(),
						address,
						value
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
