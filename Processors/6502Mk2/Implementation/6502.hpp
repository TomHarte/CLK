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

	#define restore_point()	(__COUNTER__ + ResumePoint::Max + AccessProgram::Max)

	#define join(a, b) 			a##b
	#define attach(a, b) 		join(a, b)
	#define line_label(name)	attach(name, __LINE__)

	// TODO: find a way not to generate a restore point if pause precision and uses_ready_line/model allows it.
	#define access(type, addr, value)	{																\
		static constexpr int location = restore_point();												\
		[[fallthrough]]; case location:																	\
		line_label(repeat):																				\
																										\
		if constexpr (Traits::pause_precision >= PausePrecision::AnyCycle) {							\
			if(Storage::cycles_ <= Cycles(0)) {															\
				Storage::resume_point_ = location;														\
				return;																					\
			}																							\
		}																								\
																										\
		if(Traits::uses_ready_line && (is_read(type) || is_65c02(model)) && Storage::inputs_.ready) {	\
			Storage::cycles_ -= Storage::bus_handler_.perform<BusOperation::Ready>(						\
				addr,																					\
				Data::NoValue{}																			\
			);																							\
			goto line_label(repeat);																	\
		}																								\
																										\
		Storage::cycles_ -= Storage::bus_handler_.perform<type>(addr, value);							\
	}

	#define access_program(name)	ResumePoint::Max + AccessProgram::name

	using ResumePoint = Storage::ResumePoint;
	using InterruptRequests = Storage::Inputs::InterruptRequests;

	while(true) switch(Storage::resume_point_) {
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

			access(BusOperation::ReadOpcode, Address::Literal(Storage::pc_), Storage::opcode_);
			++Storage::registers_.pc;
			access(BusOperation::Read, Address::Literal(Storage::pc_), Storage::operand_);

			Storage::decoded_ = Decoder<model>::decode(Storage::opcode_);
			Storage::resume_point_ = ResumePoint::Max + int(Storage::decoded_.access_program);
			break;

		// MARK: - Access patterns.

		case access_program(Immediate):
			++Storage::registers_.pc;
			[[fallthrough]];

		case access_program(Implied):
			perform(Storage::decoded_.operation, Storage::registers_, Storage::operand_);
			goto fetch_decode;

		// MARK: - NMI/IRQ/Reset.
		interrupt:
			read(BusOperation::Read, Address::Literal(Storage::pc_), Storage::operand_);
			read(BusOperation::Read, Address::Literal(Storage::pc_), Storage::operand_);

			if(Storage::inputs_.interrupt_requests & (InterruptRequests::Reset | InterruptRequests::PowerOn)) {
				Storage::inputs_.interrupt_requests &= ~InterruptRequests::PowerOn;
				goto reset;
			}
			assert(Storage::inputs_.interrupt_requests & (InterruptRequests::IRQ | InterruptRequests::NMI));

			--Storage::s_;
			access(BusOperation::Write, Address::Stack(Storage::s_), Storage::pc_.halves.high);
			--Storage::s_;
			access(BusOperation::Write, Address::Stack(Storage::s_), Storage::pc_.halves.low);
			--Storage::s_;
			access(BusOperation::Write, Address::Stack(Storage::s_), static_cast<uint8_t>(Storage::flags_) & ~Flag::Break);

			Storage::flags_.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) {
				Storage::flags_.decimal = 0;
			}

			if(Storage::inputs_.interrupt_requests & InterruptRequests::NMI) {
				goto nmi;
			}

			access(BusOperation::Read, Address::Vector(0xfe), Storage::pc_.halves.low);
			access(BusOperation::Read, Address::Vector(0xff), Storage::pc_.halves.high);
			goto fetch_decode;

		nmi:
			access(BusOperation::Read, Address::Vector(0xfa), Storage::pc_.halves.low);
			access(BusOperation::Read, Address::Vector(0xfb), Storage::pc_.halves.high);
			goto fetch_decode;

		reset:
			--Storage::s_;
			access(BusOperation::Read, Address::Stack(Storage::s_), Storage::operand_);
			--Storage::s_;
			access(BusOperation::Read, Address::Stack(Storage::s_), Storage::operand_);
			--Storage::s_;
			access(BusOperation::Read, Address::Stack(Storage::s_), Storage::operand_);

			Storage::flags_.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) Storage::flags_.decimal = 0;

			read(BusOperation::Read, Address::Vector(0xfc), Storage::pc_.halves.low);
			read(BusOperation::Read, Address::Vector(0xfd), Storage::pc_.halves.high);

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
