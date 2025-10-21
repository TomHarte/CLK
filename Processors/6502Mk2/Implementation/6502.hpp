//
//  6502.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Processors/6502Mk2/Decoder.hpp"

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

	#define test_cycles(precision) {							\
		static constexpr int test_location = restore_point();	\
		if constexpr (Traits::pause_precision >= precision) {	\
			if(Storage::cycles_ <= Cycles(0)) {					\
				Storage::resume_point_ = test_location;			\
				return;											\
			}													\
		}														\
		[[fallthrough]]; case test_location: (void)0;			\
	}

	// TODO: find a way not to generate a restore point if Traits::uses_ready_line is false.
	#define read(type, addr, value)	{											\
		static_assert(is_read(type));											\
		static constexpr int location = restore_point();						\
																				\
		if(Traits::uses_ready_line && Storage::inputs_.ready) {					\
			Storage::return_from_ready_ = location;								\
			goto spin_ready;													\
		}																		\
																				\
		[[fallthrough]]; case location:											\
		Storage::cycles_ -= Storage::bus_handler_.perform<type>(addr, value);	\
		test_cycles(PausePrecision::AnyCycle)									\
	}

	// TODO: test RDY if a suitable-advanced 6502.
	#define write(type, addr, value)	{										\
		static_assert(is_write(type));											\
		Storage::cycles_ -= Storage::bus_handler_.perform<type>(addr, value);	\
		test_cycles(PausePrecision::AnyCycle)									\
	}

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

			read(BusOperation::ReadOpcode, Address::Literal(Storage::pc_), Storage::opcode_);
			++Storage::pc_;
			read(BusOperation::Read, Address::Literal(Storage::pc_), Storage::operand_);

			Storage::decoded_ = Decoder<model>::decode(Storage::opcode_);
			Storage::resume_point_ = ResumePoint::Max + int(Storage::decoded_.access_program);
			break;

		// TODO: all access programs.

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
			read(BusOperation::Write, Address::Stack(Storage::s_), Storage::pc_.halves.high);
			--Storage::s_;
			read(BusOperation::Write, Address::Stack(Storage::s_), Storage::pc_.halves.low);
			--Storage::s_;
			read(BusOperation::Write, Address::Stack(Storage::s_), static_cast<uint8_t>(Storage::flags_) & ~Flag::Break);

			Storage::flags_.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) {
				Storage::flags_.decimal = 0;
			}

			if(Storage::inputs_.interrupt_requests & InterruptRequests::NMI) {
				goto nmi;
			}

			read(BusOperation::Read, Address::Vector(0xfe), Storage::pc_.halves.low);
			read(BusOperation::Read, Address::Vector(0xff), Storage::pc_.halves.high);
			goto fetch_decode;

		nmi:
			read(BusOperation::Read, Address::Vector(0xfa), Storage::pc_.halves.low);
			read(BusOperation::Read, Address::Vector(0xfb), Storage::pc_.halves.high);
			goto fetch_decode;

		reset:
			--Storage::s_;
			read(BusOperation::Read, Address::Stack(Storage::s_), Storage::operand_);
			--Storage::s_;
			read(BusOperation::Read, Address::Stack(Storage::s_), Storage::operand_);
			--Storage::s_;
			read(BusOperation::Read, Address::Stack(Storage::s_), Storage::operand_);

			Storage::flags_.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) Storage::flags_.decimal = 0;

			read(BusOperation::Read, Address::Vector(0xfc), Storage::pc_.halves.low);
			read(BusOperation::Read, Address::Vector(0xfd), Storage::pc_.halves.high);

			goto fetch_decode;

		// MARK: - Spin on RDY.
		spin_ready:
			Storage::cycles_ -= Storage::bus_handler_.perform<BusOperation::Ready>(
				Address::Literal{Storage::ready_address_},
				Data::NoValue{}
			);
			test_cycles(PausePrecision::AnyCycle);
			if(Storage::inputs_.ready) {
				goto spin_ready;
			}

			Storage::resume_point_ = Storage::return_from_ready_;
			break;

	}

	#undef write
	#undef read
	#undef test_cycles
	#undef restore_point
}

}
