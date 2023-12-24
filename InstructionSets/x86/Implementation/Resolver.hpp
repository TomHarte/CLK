//
//  Resolver.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Resolver_h
#define Resolver_h

#include "../AccessType.hpp"

namespace InstructionSet::x86 {

/// Obtain a pointer to the value desribed by @c source, which is one of those named by @c pointer, using @c instruction and @c context
/// for offsets, registers and memory contents.
///
/// If @c source is Source::None then @c none is returned.
///
/// If @c source is Source::Immediate then the appropriate portion of @c instrucion's operand
/// is copied to @c *immediate and @c immediate is returned.
template <typename IntT, AccessType access, typename InstructionT, typename ContextT>
typename Accessor<IntT, access>::type resolve(
	InstructionT &instruction,
	Source source,
	DataPointer pointer,
	ContextT &context,
	IntT *none = nullptr,
	IntT *immediate = nullptr
);

/// Calculates the absolute address for @c pointer given the registers and memory provided in @c context and taking any
/// referenced offset from @c instruction.
template <Source source, typename IntT, AccessType access, typename InstructionT, typename ContextT>
uint32_t address(
	InstructionT &instruction,
	DataPointer pointer,
	ContextT &context
) {
	if constexpr (source == Source::DirectAddress) {
		return instruction.offset();
	}

	uint32_t address;
	uint16_t zero = 0;
	address = resolve<uint16_t, AccessType::Read>(instruction, pointer.index(), pointer, context, &zero);
	if constexpr (is_32bit(ContextT::model)) {
		address <<= pointer.scale();
	}
	address += instruction.offset();

	if constexpr (source == Source::IndirectNoBase) {
		return address;
	}
	return address + resolve<uint16_t, AccessType::Read>(instruction, pointer.base(), pointer, context);
}

/// @returns a pointer to the contents of the register identified by the combination of @c IntT and @c Source if any;
/// @c nullptr otherwise. @c access is currently unused but is intended to provide the hook upon which updates to
/// segment registers can be tracked for protected modes.
template <typename IntT, AccessType access, Source source, typename ContextT>
IntT *register_(ContextT &context) {
	static constexpr bool supports_dword = is_32bit(ContextT::model);

	switch(source) {
		case Source::eAX:
			// Slightly contorted if chain here and below:
			//
			//	(i) does the `constexpr` version of a `switch`; and
			//	(i) ensures .eax() etc aren't called on @c registers for 16-bit processors, so they need not implement 32-bit storage.
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.eax();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.ax();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.al();		}
			else 																{	return nullptr;						}
		case Source::eCX:
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.ecx();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.cx();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.cl();		}
			else 																{	return nullptr;						}
		case Source::eDX:
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.edx();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.dx();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.dl();		}
			else if constexpr (std::is_same_v<IntT, uint32_t>)					{	return nullptr;						}
		case Source::eBX:
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.ebx();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.bx();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.bl();		}
			else if constexpr (std::is_same_v<IntT, uint32_t>)					{	return nullptr;						}
		case Source::eSPorAH:
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.esp();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.sp();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.ah();		}
			else																{	return nullptr;						}
		case Source::eBPorCH:
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.ebp();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.bp();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.ch();		}
			else 																{	return nullptr;						}
		case Source::eSIorDH:
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.esi();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.si();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.dh();		}
			else 																{	return nullptr;						}
		case Source::eDIorBH:
			if constexpr (supports_dword && std::is_same_v<IntT, uint32_t>) 	{	return &context.registers.edi();	}
			else if constexpr (std::is_same_v<IntT, uint16_t>)					{	return &context.registers.di();		}
			else if constexpr (std::is_same_v<IntT, uint8_t>)					{	return &context.registers.bh();		}
			else																{	return nullptr;						}

		// Segment registers are always 16-bit.
		case Source::ES:	if constexpr (std::is_same_v<IntT, uint16_t>) return &context.registers.es(); else return nullptr;
		case Source::CS:	if constexpr (std::is_same_v<IntT, uint16_t>) return &context.registers.cs(); else return nullptr;
		case Source::SS:	if constexpr (std::is_same_v<IntT, uint16_t>) return &context.registers.ss(); else return nullptr;
		case Source::DS:	if constexpr (std::is_same_v<IntT, uint16_t>) return &context.registers.ds(); else return nullptr;

		// 16-bit models don't have FS and GS.
		case Source::FS:	if constexpr (is_32bit(ContextT::model) && std::is_same_v<IntT, uint16_t>) return &context.registers.fs(); else return nullptr;
		case Source::GS:	if constexpr (is_32bit(ContextT::model) && std::is_same_v<IntT, uint16_t>) return &context.registers.gs(); else return nullptr;

		default: return nullptr;
	}
}

///Obtains the address described by @c pointer from @c instruction given the registers and memory as described by @c context.
template <typename IntT, AccessType access, typename InstructionT, typename ContextT>
uint32_t address(
	InstructionT &instruction,
	DataPointer pointer,
	ContextT &context
) {
	// TODO: at least on the 8086 this isn't how register 'addresses' are resolved; instead whatever was the last computed address
	// remains in the address register and is returned. Find out what other x86s do and make a decision.
	switch(pointer.source()) {
		default:						return 0;
		case Source::eAX:				return *register_<IntT, access, Source::eAX>(context);
		case Source::eCX:				return *register_<IntT, access, Source::eCX>(context);
		case Source::eDX:				return *register_<IntT, access, Source::eDX>(context);
		case Source::eBX:				return *register_<IntT, access, Source::eBX>(context);
		case Source::eSPorAH:			return *register_<IntT, access, Source::eSPorAH>(context);
		case Source::eBPorCH:			return *register_<IntT, access, Source::eBPorCH>(context);
		case Source::eSIorDH:			return *register_<IntT, access, Source::eSIorDH>(context);
		case Source::eDIorBH:			return *register_<IntT, access, Source::eDIorBH>(context);
		case Source::Indirect:			return address<Source::Indirect, IntT, access>(instruction, pointer, context);
		case Source::IndirectNoBase:	return address<Source::IndirectNoBase, IntT, access>(instruction, pointer, context);
		case Source::DirectAddress:		return address<Source::DirectAddress, IntT, access>(instruction, pointer, context);
	}
}

// See forward declaration, above, for details.
template <typename IntT, AccessType access, typename InstructionT, typename ContextT>
typename Accessor<IntT, access>::type resolve(
	InstructionT &instruction,
	Source source,
	DataPointer pointer,
	ContextT &context,
	IntT *none,
	IntT *immediate
) {
	// Rules:
	//
	// * if this is a memory access, set target_address and break;
	// * otherwise return the appropriate value.
	uint32_t target_address = 0;
	switch(source) {
		// Defer all register accesses to the register-specific lookup.
		case Source::eAX:		return *register_<IntT, access, Source::eAX>(context);
		case Source::eCX:		return *register_<IntT, access, Source::eCX>(context);
		case Source::eDX:		return *register_<IntT, access, Source::eDX>(context);
		case Source::eBX:		return *register_<IntT, access, Source::eBX>(context);
		case Source::eSPorAH:	return *register_<IntT, access, Source::eSPorAH>(context);
		case Source::eBPorCH:	return *register_<IntT, access, Source::eBPorCH>(context);
		case Source::eSIorDH:	return *register_<IntT, access, Source::eSIorDH>(context);
		case Source::eDIorBH:	return *register_<IntT, access, Source::eDIorBH>(context);
		case Source::ES:		return *register_<IntT, access, Source::ES>(context);
		case Source::CS:		return *register_<IntT, access, Source::CS>(context);
		case Source::SS:		return *register_<IntT, access, Source::SS>(context);
		case Source::DS:		return *register_<IntT, access, Source::DS>(context);
		case Source::FS:		return *register_<IntT, access, Source::FS>(context);
		case Source::GS:		return *register_<IntT, access, Source::GS>(context);

		case Source::None:		return *none;

		case Source::Immediate:
			*immediate = IntT(instruction.operand());
		return *immediate;

		case Source::Indirect:
			target_address = address<Source::Indirect, IntT, access>(instruction, pointer, context);
		break;
		case Source::IndirectNoBase:
			target_address = address<Source::IndirectNoBase, IntT, access>(instruction, pointer, context);
		break;
		case Source::DirectAddress:
			target_address = address<Source::DirectAddress, IntT, access>(instruction, pointer, context);
		break;
	}

	// If execution has reached here then a memory fetch is required.
	// Do it and exit.
	//
	// TODO: support 32-bit addresses.
	return context.memory.template access<IntT, access>(instruction.data_segment(), uint16_t(target_address));
}

}

#endif /* Resolver_h */
