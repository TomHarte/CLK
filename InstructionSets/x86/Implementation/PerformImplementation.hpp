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

#include "Arithmetic.hpp"
#include "BCD.hpp"
#include "FlowControl.hpp"
#include "InOut.hpp"
#include "LoadStore.hpp"
#include "Logical.hpp"
#include "Repetition.hpp"
#include "Resolver.hpp"
#include "ShiftRoll.hpp"
#include "Stack.hpp"

#include "../Interrupts.hpp"
#include "../AccessType.hpp"

//
// Comments throughout headers above come from the 1997 edition of the
// Intel Architecture Software Developer’s Manual; that year all such
// definitions still fitted within a single volume, Volume 2.
//
// Order Number 243191; e.g. https://www.ardent-tool.com/CPU/docs/Intel/IA/243191-002.pdf
//

namespace InstructionSet::x86 {

template <
	DataSize data_size,
	AddressSize address_size,
	typename InstructionT,
	typename ContextT
> void perform(
	const InstructionT &instruction,
	ContextT &context
) {
	using IntT = typename DataSizeType<data_size>::type;
	using AddressT = typename AddressSizeType<address_size>::type;

	// Establish source() and destination() shorthands to fetch data if necessary.
	//
	// C++17, which this project targets at the time of writing, does not provide templatised lambdas.
	// So the following division is in part a necessity.
	//
	// (though GCC offers C++20 syntax as an extension, and Clang seems to follow along, so maybe I'm overthinking)
	IntT immediate;
	const auto source_r = [&]() -> read_t<IntT> {
		return resolve<IntT, AccessType::Read>(
			instruction,
			instruction.source().source(),
			instruction.source(),
			context,
			nullptr,
			&immediate);
	};
	const auto source_rmw = [&]() -> modify_t<IntT> {
		return resolve<IntT, AccessType::ReadModifyWrite>(
			instruction,
			instruction.source().source(),
			instruction.source(),
			context,
			nullptr,
			&immediate);
	};
	const auto destination_r = [&]() -> read_t<IntT> {
		return resolve<IntT, AccessType::Read>(
			instruction,
			instruction.destination().source(),
			instruction.destination(),
			context,
			nullptr,
			&immediate);
	};
	const auto destination_w = [&]() -> write_t<IntT> {
		return resolve<IntT, AccessType::Write>(
			instruction,
			instruction.destination().source(),
			instruction.destination(),
			context,
			nullptr,
			&immediate);
	};
	const auto destination_rmw = [&]() -> modify_t<IntT> {
		return resolve<IntT, AccessType::ReadModifyWrite>(
			instruction,
			instruction.destination().source(),
			instruction.destination(),
			context,
			nullptr,
			&immediate);
	};

	// Performs a displacement jump only if @c condition is true.
	const auto jcc = [&](bool condition) {
		Primitive::jump(
			condition,
			instruction.displacement(),
			context);
	};

	const auto shift_count = [&]() -> uint8_t {
		static constexpr uint8_t mask = (ContextT::model != Model::i8086) ? 0x1f : 0xff;
		switch(instruction.source().source()) {
			case Source::None:		return 1;
			case Source::Immediate:	return uint8_t(instruction.operand()) & mask;
			default:				return context.registers.cl() & mask;
		}
	};

	// Some instructions use a pair of registers as an extended accumulator — DX:AX or EDX:EAX.
	// The two following return the high and low parts of that pair; they also work in Byte mode to return AH:AL,
	// i.e. AX split into high and low parts.
	const auto pair_high = [&]() -> IntT& {
		if constexpr (data_size == DataSize::Byte) 			return context.registers.ah();
		else if constexpr (data_size == DataSize::Word)		return context.registers.dx();
		else if constexpr (data_size == DataSize::DWord)	return context.registers.edx();
	};
	const auto pair_low = [&]() -> IntT& {
		if constexpr (data_size == DataSize::Byte) 			return context.registers.al();
		else if constexpr (data_size == DataSize::Word)		return context.registers.ax();
		else if constexpr (data_size == DataSize::DWord)	return context.registers.eax();
	};

	// For the string operations, evaluate to either SI and DI or ESI and EDI, depending on the address size.
	const auto eSI = [&]() -> AddressT& {
		if constexpr (std::is_same_v<AddressT, uint16_t>) {
			return context.registers.si();
		} else {
			return context.registers.esi();
		}
	};
	const auto eDI = [&]() -> AddressT& {
		if constexpr (std::is_same_v<AddressT, uint16_t>) {
			return context.registers.di();
		} else {
			return context.registers.edi();
		}
	};

	// For counts, provide either eCX or CX depending on address size.
	const auto eCX = [&]() -> AddressT& {
		if constexpr (std::is_same_v<AddressT, uint16_t>) {
			return context.registers.cx();
		} else {
			return context.registers.ecx();
		}
	};

	// Gets the port for an IN or OUT; these are always 16-bit.
	const auto port = [&](Source source) -> uint16_t {
		switch(source) {
			case Source::DirectAddress:	return instruction.offset();
			default:					return context.registers.dx();
		}
	};

	// Guide to the below:
	//
	//	* use hard-coded register names where appropriate, otherwise use the source_X() and destination_X() lambdas;
	//	* return directly if there is definitely no possible write back to RAM;
	//	* break if there's a chance of writeback.
	switch(instruction.operation()) {
		default:
			assert(false);

		case Operation::Invalid:
			// TODO: throw on higher-order processors.
		case Operation::ESC:
		case Operation::NOP:	return;

		case Operation::AAM:	Primitive::aam(context.registers.axp(), uint8_t(instruction.operand()), context);	return;
		case Operation::AAD:	Primitive::aad(context.registers.axp(), uint8_t(instruction.operand()), context);	return;
		case Operation::AAA:	Primitive::aaas<true>(context.registers.axp(), context);					return;
		case Operation::AAS:	Primitive::aaas<false>(context.registers.axp(), context);					return;
		case Operation::DAA:	Primitive::daas<true>(context.registers.al(), context);						return;
		case Operation::DAS:	Primitive::daas<false>(context.registers.al(), context);					return;

		case Operation::CBW:	Primitive::cbw(pair_low());					return;
		case Operation::CWD:	Primitive::cwd(pair_high(), pair_low());	return;

		case Operation::HLT:	context.flow_controller.halt();		return;
		case Operation::WAIT:	context.flow_controller.wait();		return;

		case Operation::ADC:
			Primitive::add<true, IntT>(destination_rmw(), source_r(), context);
		break;
		case Operation::ADD:
			Primitive::add<false, IntT>(destination_rmw(), source_r(), context);
		break;
		case Operation::SBB:
			Primitive::sub<true, AccessType::ReadModifyWrite, IntT>(destination_rmw(), source_r(), context);
		break;
		case Operation::SUB:
			Primitive::sub<false, AccessType::ReadModifyWrite, IntT>(destination_rmw(), source_r(), context);
		break;
		case Operation::CMP:
			Primitive::sub<false, AccessType::Read, IntT>(destination_r(), source_r(), context);
		return;
		case Operation::TEST:
			Primitive::test<IntT>(destination_r(), source_r(), context);
		return;

		case Operation::MUL:		Primitive::mul<IntT>(pair_high(), pair_low(), source_r(), context);			return;
		case Operation::IMUL_1:		Primitive::imul<IntT>(pair_high(), pair_low(), source_r(), context);		return;
		case Operation::DIV:		Primitive::div<IntT>(pair_high(), pair_low(), source_r(), context);			return;
		case Operation::IDIV:		Primitive::idiv<false, IntT>(pair_high(), pair_low(), source_r(), context);	return;
		case Operation::IDIV_REP:
			if constexpr (ContextT::model == Model::i8086) {
				Primitive::idiv<true, IntT>(pair_high(), pair_low(), source_r(), context);
				break;
			} else {
				static_assert(int(Operation::IDIV_REP) == int(Operation::LEAVE));
				if constexpr (std::is_same_v<IntT, uint16_t> || std::is_same_v<IntT, uint32_t>) {
					Primitive::leave<IntT>();
				}
			}
		return;

		case Operation::INC:	Primitive::inc<IntT>(destination_rmw(), context);		break;
		case Operation::DEC:	Primitive::dec<IntT>(destination_rmw(), context);		break;

		case Operation::AND:	Primitive::and_<IntT>(destination_rmw(), source_r(), context);		break;
		case Operation::OR:		Primitive::or_<IntT>(destination_rmw(), source_r(), context);		break;
		case Operation::XOR:	Primitive::xor_<IntT>(destination_rmw(), source_r(), context);		break;
		case Operation::NEG:	Primitive::neg<IntT>(source_rmw(), context);						break;	// TODO: should be a destination.
		case Operation::NOT:	Primitive::not_<IntT>(source_rmw());								break;	// TODO: should be a destination.

		case Operation::CALLrel:
			Primitive::call_relative<AddressT>(instruction.displacement(), context);
		return;
		case Operation::CALLabs:	Primitive::call_absolute<IntT>(destination_r(), context);					return;
		case Operation::CALLfar:	Primitive::call_far(instruction, context);									return;

		case Operation::JMPrel:	jcc(true);														return;
		case Operation::JMPabs:	Primitive::jump_absolute<IntT>(destination_r(), context);		return;
		case Operation::JMPfar:	Primitive::jump_far(instruction, context);						return;

		case Operation::JCXZ:	jcc(!eCX());														return;
		case Operation::LOOP:	Primitive::loop<AddressT>(eCX(), instruction.offset(), context);	return;
		case Operation::LOOPE:	Primitive::loope<AddressT>(eCX(), instruction.offset(), context);	return;
		case Operation::LOOPNE:	Primitive::loopne<AddressT>(eCX(), instruction.offset(), context);	return;

		case Operation::IRET:		Primitive::iret(context);					return;
		case Operation::RETnear:	Primitive::ret_near(instruction, context);	return;
		case Operation::RETfar:		Primitive::ret_far(instruction, context);	return;

		case Operation::INT:	interrupt(instruction.operand(), context);		return;
		case Operation::INTO:	Primitive::into(context);						return;

		case Operation::SAHF:	Primitive::sahf(context.registers.ah(), context);		return;
		case Operation::LAHF:	Primitive::lahf(context.registers.ah(), context);		return;

		case Operation::LDS:
			if constexpr (data_size == DataSize::Word) {
				Primitive::ld<Source::DS>(instruction, destination_w(), context);
				context.segments.did_update(Source::DS);
			}
		return;
		case Operation::LES:
			if constexpr (data_size == DataSize::Word) {
				Primitive::ld<Source::ES>(instruction, destination_w(), context);
				context.segments.did_update(Source::ES);
			}
		return;

		case Operation::LEA:	Primitive::lea<IntT>(instruction, destination_w(), context);	return;
		case Operation::MOV:
			Primitive::mov<IntT>(destination_w(), source_r());
			if constexpr (std::is_same_v<IntT, uint16_t>) {
				context.segments.did_update(instruction.destination().source());
			}
		break;

		case Operation::JO:		jcc(context.flags.template condition<Condition::Overflow>());		return;
		case Operation::JNO:	jcc(!context.flags.template condition<Condition::Overflow>());		return;
		case Operation::JB:		jcc(context.flags.template condition<Condition::Below>());			return;
		case Operation::JNB:	jcc(!context.flags.template condition<Condition::Below>());			return;
		case Operation::JZ:		jcc(context.flags.template condition<Condition::Zero>());			return;
		case Operation::JNZ:	jcc(!context.flags.template condition<Condition::Zero>());			return;
		case Operation::JBE:	jcc(context.flags.template condition<Condition::BelowOrEqual>());	return;
		case Operation::JNBE:	jcc(!context.flags.template condition<Condition::BelowOrEqual>());	return;
		case Operation::JS:		jcc(context.flags.template condition<Condition::Sign>());			return;
		case Operation::JNS:	jcc(!context.flags.template condition<Condition::Sign>());			return;
		case Operation::JP:		jcc(!context.flags.template condition<Condition::ParityOdd>());		return;
		case Operation::JNP:	jcc(context.flags.template condition<Condition::ParityOdd>());		return;
		case Operation::JL:		jcc(context.flags.template condition<Condition::Less>());			return;
		case Operation::JNL:	jcc(!context.flags.template condition<Condition::Less>());			return;
		case Operation::JLE:	jcc(context.flags.template condition<Condition::LessOrEqual>());	return;
		case Operation::JNLE:	jcc(!context.flags.template condition<Condition::LessOrEqual>());	return;

		case Operation::RCL:	Primitive::rcl<IntT>(destination_rmw(), shift_count(), context);	break;
		case Operation::RCR:	Primitive::rcr<IntT>(destination_rmw(), shift_count(), context);	break;
		case Operation::ROL:	Primitive::rol<IntT>(destination_rmw(), shift_count(), context);	break;
		case Operation::ROR:	Primitive::ror<IntT>(destination_rmw(), shift_count(), context);	break;
		case Operation::SAL:	Primitive::sal<IntT>(destination_rmw(), shift_count(), context);	break;
		case Operation::SAR:	Primitive::sar<IntT>(destination_rmw(), shift_count(), context);	break;
		case Operation::SHR:	Primitive::shr<IntT>(destination_rmw(), shift_count(), context);	break;

		case Operation::CLC:	Primitive::clc(context);				return;
		case Operation::CLD:	Primitive::cld(context);				return;
		case Operation::CLI:	Primitive::cli(context);				return;
		case Operation::STC:	Primitive::stc(context);				return;
		case Operation::STD:	Primitive::std(context);				return;
		case Operation::STI:	Primitive::sti(context);				return;
		case Operation::CMC:	Primitive::cmc(context);				return;

		case Operation::XCHG:	Primitive::xchg<IntT>(destination_rmw(), source_rmw());		break;

		case Operation::SALC:	Primitive::salc(context.registers.al(), context);			return;
		case Operation::SETMO:
			if constexpr (ContextT::model == Model::i8086) {
				Primitive::setmo<IntT>(destination_w(), context);
				break;
			} else {
				static_assert(int(Operation::SETMO) == int(Operation::ENTER));
				Primitive::enter<IntT>(instruction, context);
			}
		return;
		case Operation::SETMOC:
			if constexpr (ContextT::model == Model::i8086) {
				// Test CL out here to avoid taking a reference to memory if
				// no write is going to occur.
				if(context.registers.cl()) {
					Primitive::setmo<IntT>(destination_w(), context);
				}
				break;
			} else {
				static_assert(int(Operation::SETMOC) == int(Operation::BOUND));
				Primitive::bound<IntT>(instruction, destination_r(), source_r(), context);
			}
		return;

		case Operation::OUT: Primitive::out<IntT>(port(instruction.destination().source()), pair_low(), context);	return;
		case Operation::IN:	 Primitive::in<IntT>(port(instruction.source().source()), pair_low(), context);			return;

		case Operation::XLAT:	Primitive::xlat<AddressT>(instruction, context);		return;

		case Operation::POP:
			destination_w() = Primitive::pop<IntT, false>(context);
			if constexpr (std::is_same_v<IntT, uint16_t>) {
				context.segments.did_update(instruction.destination().source());
			}
		break;
		case Operation::PUSH:
			Primitive::push<IntT, false>(source_rmw(), context);	// PUSH SP modifies SP before pushing it;
																	// hence PUSH is sometimes read-modify-write.
		break;

		case Operation::POPF:
			if constexpr (std::is_same_v<IntT, uint16_t> || std::is_same_v<IntT, uint32_t>) {
				Primitive::popf(context);
			}
		return;
		case Operation::PUSHF:
			if constexpr (std::is_same_v<IntT, uint16_t> || std::is_same_v<IntT, uint32_t>) {
				Primitive::pushf(context);
			}
		return;
		case Operation::POPA:
			if constexpr (std::is_same_v<IntT, uint16_t> || std::is_same_v<IntT, uint32_t>) {
				Primitive::popa<IntT>(context);
			}
		return;
		case Operation::PUSHA:
			if constexpr (std::is_same_v<IntT, uint16_t> || std::is_same_v<IntT, uint32_t>) {
				Primitive::pusha<IntT>(context);
			}
		return;

		case Operation::CMPS:
			Primitive::cmps<IntT, AddressT, Repetition::None>(instruction, eCX(), eSI(), eDI(), context);
		return;
		case Operation::CMPS_REPE:
			Primitive::cmps<IntT, AddressT, Repetition::RepE>(instruction, eCX(), eSI(), eDI(), context);
		return;
		case Operation::CMPS_REPNE:
			Primitive::cmps<IntT, AddressT, Repetition::RepNE>(instruction, eCX(), eSI(), eDI(), context);
		return;

		case Operation::SCAS:
			Primitive::scas<IntT, AddressT, Repetition::None>(eCX(), eDI(), pair_low(), context);
		return;
		case Operation::SCAS_REPE:
			Primitive::scas<IntT, AddressT, Repetition::RepE>(eCX(), eDI(), pair_low(), context);
		return;
		case Operation::SCAS_REPNE:
			Primitive::scas<IntT, AddressT, Repetition::RepNE>(eCX(), eDI(), pair_low(), context);
		return;

		case Operation::LODS:
			Primitive::lods<IntT, AddressT, Repetition::None>(instruction, eCX(), eSI(), pair_low(), context);
		return;
		case Operation::LODS_REP:
			Primitive::lods<IntT, AddressT, Repetition::Rep>(instruction, eCX(), eSI(), pair_low(), context);
		return;

		case Operation::MOVS:
			Primitive::movs<IntT, AddressT, Repetition::None>(instruction, eCX(), eSI(), eDI(), context);
		break;
		case Operation::MOVS_REP:
			Primitive::movs<IntT, AddressT, Repetition::Rep>(instruction, eCX(), eSI(), eDI(), context);
		break;

		case Operation::STOS:
			Primitive::stos<IntT, AddressT, Repetition::None>(eCX(), eDI(), pair_low(), context);
		break;
		case Operation::STOS_REP:
			Primitive::stos<IntT, AddressT, Repetition::Rep>(eCX(), eDI(), pair_low(), context);
		break;

		case Operation::OUTS:
			Primitive::outs<IntT, AddressT, Repetition::None>(instruction, eCX(), context.registers.dx(), eSI(), context);
		return;
		case Operation::OUTS_REP:
			Primitive::outs<IntT, AddressT, Repetition::Rep>(instruction, eCX(), context.registers.dx(), eSI(), context);
		return;

		case Operation::INS:
			Primitive::ins<IntT, AddressT, Repetition::None>(eCX(), context.registers.dx(), eDI(), context);
		break;
		case Operation::INS_REP:
			Primitive::ins<IntT, AddressT, Repetition::Rep>(eCX(), context.registers.dx(), eDI(), context);
		break;
	}

	// Write to memory if required to complete this operation.
	//
	// This is not currently handled via RAII because of the amount of context that would need to place onto the stack;
	// instead code has been set up to make sure there is only at most one writeable target on loan for potential
	// write back. I might flip-flop on this, especially if I can verify whether extra stack context is easily
	// optimised out.
	context.memory.template write_back<IntT>();
}

//
// Public function; just a trampoline into a version of perform templated on data and address size.
//
// Which, yes, means there's an outer switch leading to an inner switch, which could be reduced to one big switch.
// It'd be a substantial effort to find the most neat expression of that, I think, so it is not currently done.
//
template <
	typename InstructionT,
	typename ContextT
> void perform(
	const InstructionT &instruction,
	ContextT &context
) {
	auto size = [](DataSize operation_size, AddressSize address_size) constexpr -> int {
		return int(operation_size) + (int(address_size) << 2);
	};

	// Dispatch to a function specialised on data and address size.
	switch(size(instruction.operation_size(), instruction.address_size())) {
		// 16-bit combinations.
		case size(DataSize::Byte, AddressSize::b16):
			perform<DataSize::Byte, AddressSize::b16>(instruction, context);
		return;
		case size(DataSize::Word, AddressSize::b16):
			perform<DataSize::Word, AddressSize::b16>(instruction, context);
		return;

		// 32-bit combinations.
		//
		// The if constexprs below ensure that `perform` isn't compiled for incompatible data or address size and
		// model combinations. So if a caller nominates a 16-bit model it can supply registers and memory objects
		// that don't implement 32-bit registers or accesses.
		case size(DataSize::Byte, AddressSize::b32):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::Byte, AddressSize::b32>(instruction, context);
				return;
			}
		break;
		case size(DataSize::Word, AddressSize::b32):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::Word, AddressSize::b32>(instruction, context);
				return;
			}
		break;
		case size(DataSize::DWord, AddressSize::b16):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::DWord, AddressSize::b16>(instruction, context);
				return;
			}
		break;
		case size(DataSize::DWord, AddressSize::b32):
			if constexpr (is_32bit(ContextT::model)) {
				perform<DataSize::DWord, AddressSize::b32>(instruction, context);
				return;
			}
		break;

		default: break;
	}

	// This is reachable only if the data and address size combination in use isn't available
	// on the processor model nominated.
	assert(false);
}

template <
	typename ContextT
> void interrupt(
	int index,
	ContextT &context
) {
	const uint32_t address = static_cast<uint32_t>(index) << 2;
	context.memory.preauthorise_read(address, sizeof(uint16_t) * 2);
	context.memory.preauthorise_stack_write(sizeof(uint16_t) * 3);

	const uint16_t ip = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(address);
	const uint16_t cs = context.memory.template access<uint16_t, AccessType::PreauthorisedRead>(address + 2);

	auto flags = context.flags.get();
	Primitive::push<uint16_t, true>(flags, context);
	context.flags.template set_from<Flag::Interrupt, Flag::Trap>(0);

	// Push CS and IP.
	Primitive::push<uint16_t, true>(context.registers.cs(), context);
	Primitive::push<uint16_t, true>(context.registers.ip(), context);

	// Set new destination.
	context.flow_controller.jump(cs, ip);
}

}

#endif /* PerformImplementation_h */
