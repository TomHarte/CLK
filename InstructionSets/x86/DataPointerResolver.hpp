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
		template <typename DataT> static DataT read(
			RegistersT &registers,
			MemoryT &memory,
			const Instruction<is_32bit(model)> &instruction,
			DataPointer pointer,
			typename Instruction<is_32bit(model)>::ImmediateT memory_mask = ~0) {
				DataT result;
				access<true>(registers, memory, instruction, pointer, memory_mask, result);
				return result;
			}

		template <typename DataT> static void write(
			RegistersT &registers,
			MemoryT &memory,
			const Instruction<is_32bit(model)> &instruction,
			DataPointer pointer,
			DataT value,
			typename Instruction<is_32bit(model)>::ImmediateT memory_mask = ~0) {
				access<false>(registers, memory, instruction, pointer, memory_mask, value);
			}

	private:
		template <bool is_write, typename DataT> static void access(
			RegistersT &registers,
			MemoryT &memory,
			const Instruction<is_32bit(model)> &instruction,
			DataPointer pointer,
			typename Instruction<is_32bit(model)>::ImmediateT memory_mask,
			DataT &value) {
				const Source source = pointer.source();

#define read_or_write(v, x, allow_write)	\
	case Source::x:	\
		if constexpr(allow_write && is_write) {\
			registers.template write<DataT, register_for_source<DataT>(Source::x)>(v);	\
		} else {	\
			value = registers.template read<DataT, register_for_source<DataT>(Source::x)>(); \
		}	\
	break;

#define ALLREGS(v)	f(v, eAX); f(v, eCX); f(v, eDX); f(v, eBX); \
					f(v, eSPorAH); f(v, eBPorCH); f(v, eSIorDH); f(v, eDIorBH); \
					f(v, ES); f(v, CS); f(v, SS); f(v, DS); f(v, FS); f(v, GS);

			switch(source) {
				default:
					if constexpr (!is_write) {
						value = 0;
					}
				return;

#define f(x, y) read_or_write(x, y, true)
				ALLREGS(value);
#undef f

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
					uint32_t base = 0, index = 0;

#define f(x, y) read_or_write(x, y, false)
					switch(pointer.base()) {
						default: break;
						ALLREGS(base);
					}

					switch(pointer.index()) {
						default: break;
						ALLREGS(index);
					}
#undef f

					if constexpr (model >= Model::i80386) {
						index <<= pointer.scale();
					} else {
						assert(!pointer.scale());
					}

					// TODO: verify application of memory_mask here.
					const uint32_t address = (base & memory_mask) + (index & memory_mask);

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
#undef ALLREGS
		}

		template <typename DataT> constexpr static Register register_for_source(Source source) {
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
};

}
}

#endif /* DataPointerResolver_hpp */
