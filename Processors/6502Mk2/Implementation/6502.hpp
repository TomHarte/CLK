//
//  6502.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/6502Mk2/Decoder.hpp"
#include "Processors/6502Mk2/Perform.hpp"

#include <cassert>

namespace CPU::MOS6502Mk2 {

template <Model model, typename Traits>
void Processor<model, Traits>::restart_operation_fetch() {
	Storage::resume_point_ = Storage::ResumePoint::FetchDecode;
}

template <Model model, typename Traits>
void Processor<model, Traits>::run_for(const Cycles cycles) {
	Storage::cycles_ += cycles;
	if(Storage::cycles_ <= Cycles(0)) return;

	#define restore_point()	(__COUNTER__ + int(ResumePoint::Max) + int(AccessProgram::Max))

	#define join(a, b) 			a##b
	#define attach(a, b) 		join(a, b)
	#define access_label()		attach(repeat, __LINE__)

	// TODO: find a way not to generate a restore point if pause precision and uses_ready_line/model allows it.
	#define access(type, addr, value)	{																\
		static constexpr int location = restore_point();												\
		[[fallthrough]]; case location:																	\
		[[maybe_unused]] access_label():																\
																										\
		if constexpr (Traits::pause_precision >= PausePrecision::AnyCycle) {							\
			if(Storage::cycles_ <= Cycles(0)) {															\
				Storage::resume_point_ = location;														\
				return;																					\
			}																							\
		}																								\
																										\
		if(Traits::uses_ready_line && (is_read(type) || is_65c02(model)) && Storage::inputs_.ready) {	\
			Storage::cycles_ -= Storage::bus_handler_.template perform<BusOperation::Ready>(			\
				addr,																					\
				Data::NoValue{}																			\
			);																							\
			goto access_label();																		\
		}																								\
																										\
		Storage::cycles_ -= Storage::bus_handler_.template perform<type>(addr, value);					\
	}

	#define access_program(name)	int(ResumePoint::Max) + int(AccessProgram::name)

	using ResumePoint = Storage::ResumePoint;
	using InterruptRequest = Storage::Inputs::InterruptRequest;

	while(true) switch(Storage::resume_point_) {
		default:
			__builtin_unreachable();

		// MARK: - Fetch/decode.
		fetch_decode:
		case ResumePoint::FetchDecode:

			// Pause precision will always be at least operation by operation.
			if(Storage::cycles_ <= Cycles(0)) {
				Storage::resume_point_ = ResumePoint::FetchDecode;
				return;
			}

			if(Storage::inputs_.interrupt_requests) {
				goto interrupt;
			}

			access(BusOperation::ReadOpcode, Address::Literal(Storage::registers_.pc.full), Storage::opcode_);
			++Storage::registers_.pc.full;
			access(BusOperation::Read, Address::Literal(Storage::registers_.pc.full), Storage::operand_);

			Storage::decoded_ = Decoder<model>::decode(Storage::opcode_);
			Storage::resume_point_ = ResumePoint::Max + int(Storage::decoded_.program);
			break;

		// MARK: - Access patterns.

		case access_program(Immediate):
			++Storage::registers_.pc.full;
			[[fallthrough]];

		case access_program(Implied):
			perform<model>(Storage::decoded_.operation, Storage::registers_, Storage::operand_, Storage::opcode_);
			goto fetch_decode;

		// MARK: - Stack.

		case access_program(Pull):
			access(BusOperation::Read, Address::Stack(Storage::registers_.s), Storage::operand_);
			++Storage::registers_.s;
			perform<model>(Storage::decoded_.operation, Storage::registers_, Storage::operand_, Storage::opcode_);
			goto fetch_decode;

		case access_program(Push):
			perform<model>(Storage::decoded_.operation, Storage::registers_, Storage::operand_, Storage::opcode_);
			--Storage::registers_.s;
			access(BusOperation::Write, Address::Stack(Storage::registers_.s), Storage::operand_);
			goto fetch_decode;


		// MARK: - NMI/IRQ/Reset.
		interrupt:
			access(BusOperation::Read, Address::Literal(Storage::registers_.pc.full), Storage::operand_);
			access(BusOperation::Read, Address::Literal(Storage::registers_.pc.full), Storage::operand_);

			if(Storage::inputs_.interrupt_requests & (InterruptRequest::Reset | InterruptRequest::PowerOn)) {
				Storage::inputs_.interrupt_requests &= ~InterruptRequest::PowerOn;
				goto reset;
			}
			assert(Storage::inputs_.interrupt_requests & (InterruptRequest::IRQ | InterruptRequest::NMI));

			--Storage::registers_.s;
			access(BusOperation::Write, Address::Stack(Storage::registers_.s), Storage::registers_.pc.halves.high);
			--Storage::registers_.s;
			access(BusOperation::Write, Address::Stack(Storage::registers_.s), Storage::registers_.pc.halves.low);
			--Storage::registers_.s;
			access(
				BusOperation::Write,
				Address::Stack(Storage::registers_.s),
				static_cast<uint8_t>(Storage::registers_.flags) & ~Flag::Break
			);

			Storage::registers_.flags.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) {
				Storage::flags_.decimal = 0;
			}

			if(Storage::inputs_.interrupt_requests & InterruptRequest::NMI) {
				goto nmi;
			}

			access(BusOperation::Read, Address::Vector(0xfe), Storage::registers_.pc.halves.low);
			access(BusOperation::Read, Address::Vector(0xff), Storage::registers_.pc.halves.high);
			goto fetch_decode;

		nmi:
			access(BusOperation::Read, Address::Vector(0xfa), Storage::registers_.pc.halves.low);
			access(BusOperation::Read, Address::Vector(0xfb), Storage::registers_.pc.halves.high);
			goto fetch_decode;

		reset:
			--Storage::registers_.s;
			access(BusOperation::Read, Address::Stack(Storage::registers_.s), Storage::operand_);
			--Storage::registers_.s;
			access(BusOperation::Read, Address::Stack(Storage::registers_.s), Storage::operand_);
			--Storage::registers_.s;
			access(BusOperation::Read, Address::Stack(Storage::registers_.s), Storage::operand_);

			Storage::registers_.flags.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) Storage::flags_.decimal = 0;

			access(BusOperation::Read, Address::Vector(0xfc), Storage::registers_.pc.halves.low);
			access(BusOperation::Read, Address::Vector(0xfd), Storage::registers_.pc.halves.high);

			goto fetch_decode;
	}

	#undef access_program
	#undef access
	#undef restore_point
	#undef line_label
	#undef attach
	#undef join
}

}
