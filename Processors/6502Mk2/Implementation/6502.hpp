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
	auto &registers = Storage::registers_;
	uint8_t throwaway = 0;

	const auto check_interrupt = [] {
	};
	const auto perform_operation = [&] {
		CPU::MOS6502Mk2::perform<model>(Storage::decoded_.operation, registers, Storage::operand_, Storage::opcode_);
	};

	using Literal = Address::Literal;
	using ZeroPage = Address::ZeroPage;
	using Stack = Address::Stack;
	using Vector = Address::Vector;

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

			access(BusOperation::ReadOpcode, Literal(registers.pc.full), Storage::opcode_);
			++registers.pc.full;
			check_interrupt();
			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_);

			Storage::decoded_ = Decoder<model>::decode(Storage::opcode_);
			Storage::resume_point_ = ResumePoint::Max + int(Storage::decoded_.program);
			break;

		// MARK: - Immediate, Implied, Accumulator.

		case access_program(Immediate):
			++registers.pc.full;
			[[fallthrough]];

		case access_program(Implied):
			perform_operation();
			goto fetch_decode;

		case access_program(Accumulator):
			CPU::MOS6502Mk2::perform<model>(Storage::decoded_.operation, registers, registers.a, Storage::opcode_);
			goto fetch_decode;

		// MARK: - Zero.

		case access_program(ZeroRead):
			++registers.pc.full;

			check_interrupt();
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::operand_);
			perform_operation();

			goto fetch_decode;

		case access_program(ZeroWrite):
			++registers.pc.full;

			check_interrupt();
			Storage::address_.halves.low = Storage::operand_;
			perform_operation();
			access(BusOperation::Write, ZeroPage(Storage::address_.halves.low), Storage::operand_);

			goto fetch_decode;

		case access_program(ZeroModify):
			++registers.pc.full;

			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, ZeroPage(Storage::address_.halves.low), Storage::operand_);

			access(BusOperation::Write, ZeroPage(Storage::address_.halves.low), Storage::operand_);

			check_interrupt();
			perform_operation();
			access(BusOperation::Write, ZeroPage(Storage::address_.halves.low), Storage::operand_);

			goto fetch_decode;

		// MARK: - Absolute.

		case access_program(AbsoluteRead):
			++registers.pc.full;

			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			++registers.pc.full;

			check_interrupt();
			access(BusOperation::Read, Literal(Storage::address_.full), Storage::operand_);
			perform_operation();

			goto fetch_decode;

		case access_program(AbsoluteWrite):
			++registers.pc.full;

			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			++registers.pc.full;

			check_interrupt();
			perform_operation();
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);

			goto fetch_decode;

		case access_program(AbsoluteModify):
			++registers.pc.full;

			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			++registers.pc.full;

			access(BusOperation::Read, Literal(Storage::address_.full), Storage::operand_);
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);

			check_interrupt();
			perform_operation();
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);

			goto fetch_decode;

		// MARK: - Stack.

		case access_program(Pull):
			check_interrupt();
			access(BusOperation::Read, Stack(registers.inc_s()), Storage::operand_);
			perform_operation();
			goto fetch_decode;

		case access_program(Push):
			perform_operation();
			check_interrupt();
			access(BusOperation::Write, Stack(registers.dec_s()), Storage::operand_);
			goto fetch_decode;

		// MARK: - Indexed indirect.

		case access_program(IndirectIndexedRead):
			++registers.pc.full;
			access(BusOperation::Read, ZeroPage(Storage::operand_), throwaway);
			Storage::operand_ += registers.x;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low);
			++Storage::operand_;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);
			check_interrupt();
			access(BusOperation::Read, Literal(Storage::address_.full), Storage::operand_);
			perform_operation();
			goto fetch_decode;

		case access_program(IndirectIndexedWrite):
			++registers.pc.full;
			access(BusOperation::Read, ZeroPage(Storage::operand_), throwaway);
			Storage::operand_ += registers.x;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low);
			++Storage::operand_;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);
			check_interrupt();
			perform_operation();
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);
			goto fetch_decode;

		case access_program(IndirectIndexedModify):
			++registers.pc.full;
			access(BusOperation::Read, ZeroPage(Storage::operand_), throwaway);
			Storage::operand_ += registers.x;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low);
			++Storage::operand_;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);
			access(BusOperation::Read, Literal(Storage::address_.full), Storage::operand_);
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);
			check_interrupt();
			perform_operation();
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);
			goto fetch_decode;


		// MARK: - JAM

		case access_program(JAM):
			access(BusOperation::Read, Vector(0xff), throwaway);
			access(BusOperation::Read, Vector(0xfe), throwaway);
			access(BusOperation::Read, Vector(0xfe), throwaway);

			Storage::resume_point_ = ResumePoint::Jam;
			[[fallthrough]];
		case ResumePoint::Jam:
		jammed:
			if(Storage::cycles_ <= Cycles(0)) {
				return;
			}
			Storage::cycles_ -= Storage::bus_handler_.template perform<BusOperation::Read>(
				Vector(0xff),
				throwaway
			);
			goto jammed;

		// MARK: - NMI/IRQ/Reset, and BRK.
		case access_program(BRK):
			++registers.pc.full;
			access(BusOperation::Write, Stack(registers.dec_s()), registers.pc.halves.high);
			access(BusOperation::Write, Stack(registers.dec_s()), registers.pc.halves.low);
			access(
				BusOperation::Write,
				Stack(registers.dec_s()),
				static_cast<uint8_t>(registers.flags) | Flag::Break
			);

			registers.flags.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) {
				registers.flags.decimal = 0;
			}

			access(BusOperation::Read, Vector(0xfe), registers.pc.halves.low);
			check_interrupt();
			access(BusOperation::Read, Vector(0xff), registers.pc.halves.high);
			goto fetch_decode;

		interrupt:
			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_);
			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_);

			if(Storage::inputs_.interrupt_requests & (InterruptRequest::Reset | InterruptRequest::PowerOn)) {
				Storage::inputs_.interrupt_requests &= ~InterruptRequest::PowerOn;
				goto reset;
			}
			assert(Storage::inputs_.interrupt_requests & (InterruptRequest::IRQ | InterruptRequest::NMI));

			access(BusOperation::Write, Stack(registers.dec_s()), registers.pc.halves.high);
			access(BusOperation::Write, Stack(registers.dec_s()), registers.pc.halves.low);
			access(
				BusOperation::Write,
				Stack(registers.dec_s()),
				static_cast<uint8_t>(registers.flags) & ~Flag::Break
			);

			registers.flags.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) registers.flags.decimal = 0;

			if(Storage::inputs_.interrupt_requests & InterruptRequest::NMI) {
				goto nmi;
			}

			access(BusOperation::Read, Vector(0xfe), registers.pc.halves.low);
			check_interrupt();
			access(BusOperation::Read, Vector(0xff), registers.pc.halves.high);
			goto fetch_decode;

		nmi:
			access(BusOperation::Read, Vector(0xfa), registers.pc.halves.low);
			check_interrupt();
			access(BusOperation::Read, Vector(0xfb), registers.pc.halves.high);
			goto fetch_decode;

		reset:
			access(BusOperation::Read, Stack(registers.dec_s()), Storage::operand_);
			access(BusOperation::Read, Stack(registers.dec_s()), Storage::operand_);
			access(BusOperation::Read, Stack(registers.dec_s()), Storage::operand_);

			registers.flags.inverse_interrupt = 0;
			if constexpr (is_65c02(model)) registers.flags.decimal = 0;

			access(BusOperation::Read, Vector(0xfc), registers.pc.halves.low);
			check_interrupt();
			access(BusOperation::Read, Vector(0xfd), registers.pc.halves.high);
			goto fetch_decode;
	}

	#undef perform
	#undef access_program
	#undef access
	#undef restore_point
	#undef line_label
	#undef attach
	#undef join
}

}
