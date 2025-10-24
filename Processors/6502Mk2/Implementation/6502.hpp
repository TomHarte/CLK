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

	#define restore_point()	(__COUNTER__ + int(ResumePoint::Max) + int(AddressingMode::Max))

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

	#define access_program(name)	int(ResumePoint::Max) + int(AddressingMode::name)

	using ResumePoint = Storage::ResumePoint;
	using InterruptRequest = Storage::Inputs::InterruptRequest;
	auto &registers = Storage::registers_;
	uint8_t throwaway = 0;

	const auto index = [&] {
		return Storage::decoded_.index == Index::X ? registers.x : registers.y;
	};
	const auto check_interrupt = [] {
		// TODO.
	};
	const auto perform_operation = [&] {
		CPU::MOS6502Mk2::perform<model>(
			Storage::decoded_.operation,
			registers,
			Storage::operand_,
			Storage::opcode_
		);
	};

	using Literal = Address::Literal;
	using ZeroPage = Address::ZeroPage;
	using Stack = Address::Stack;
	using Vector = Address::Vector;

	while(true) switch(Storage::resume_point_) {
		default:
			__builtin_unreachable();

		// MARK: - Read, write or modify accesses.

		access_zero:
			if constexpr (is_65c02(model)) {
				if(Storage::decoded_.operation == Operation::FastNOP) {
					goto fetch_decode;
				}
			}

			if(Storage::decoded_.type == Type::Write) {
				goto access_zero_write;
			}

			// Read.
			check_interrupt();
			access(BusOperation::Read, ZeroPage(Storage::address_.halves.low), Storage::operand_);
			if(Storage::decoded_.type == Type::Read) {
				perform_operation();
				goto fetch_decode;
			}

			// Modify stall.
			access(
				is_65c02(model) ? BusOperation::Read : BusOperation::Write,
				ZeroPage(Storage::address_.halves.low),
				Storage::operand_
			);

			// Write.
		access_zero_write:
			check_interrupt();
			perform_operation();
			access(BusOperation::Write, ZeroPage(Storage::address_.halves.low), Storage::operand_);

			goto fetch_decode;

		access_absolute:
			if constexpr (is_65c02(model)) {
				if(Storage::decoded_.operation == Operation::FastNOP) {
					goto fetch_decode;
				}
			}
			if(Storage::decoded_.type == Type::Write) {
				goto access_absolute_write;
			}

			// Read.
			check_interrupt();
			access(BusOperation::Read, Literal(Storage::address_.full), Storage::operand_);
			if(Storage::decoded_.type == Type::Read) {
				perform_operation();
				goto fetch_decode;
			}

			// Modify stall.
			access(
				is_65c02(model) ? BusOperation::Read : BusOperation::Write,
				Literal(Storage::address_.full),
				Storage::operand_
			);

			// Write.
		access_absolute_write:
			check_interrupt();
			perform_operation();
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);

			goto fetch_decode;

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

			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			access(BusOperation::ReadOpcode, Literal(registers.pc.full), Storage::opcode_);
			++registers.pc.full;
			Storage::decoded_ = Decoder<model>::decode(Storage::opcode_);

			// 65c02 special case: support single-cycle NOPs.
			if constexpr (is_65c02(model)) {
				if(
					Storage::decoded_.mode == AddressingMode::Implied &&
					Storage::decoded_.operation == Operation::FastNOP
				) {
					goto fetch_decode;
				}
			}

			check_interrupt();
			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_);

			Storage::resume_point_ = ResumePoint::Max + int(Storage::decoded_.mode);
			break;

		// MARK: - Immediate, Implied, Accumulator.

		case access_program(Immediate):
			++registers.pc.full;
			[[fallthrough]];

		case access_program(Implied):
			perform_operation();
			goto fetch_decode;

		case access_program(Accumulator):
			CPU::MOS6502Mk2::perform<model>(
				Storage::decoded_.operation,
				registers,
				registers.a,
				Storage::opcode_
			);
			goto fetch_decode;

		// MARK: - Stack.

		case access_program(Pull):
			access(BusOperation::Read, Stack(registers.s), Storage::operand_);
			check_interrupt();
			access(BusOperation::Read, Stack(registers.inc_s()), Storage::operand_);
			perform_operation();
			goto fetch_decode;

		case access_program(Push):
			check_interrupt();
			perform_operation();
			access(BusOperation::Write, Stack(registers.dec_s()), Storage::operand_);
			goto fetch_decode;

		// MARK: - Relative.

		case access_program(Relative):
			++registers.pc.full;

			if(!test(Storage::decoded_.operation, registers)) {
				goto fetch_decode;
			}

			Storage::address_ = registers.pc;
			access(BusOperation::Read, Literal(registers.pc.full), throwaway);

			registers.pc.full += int8_t(Storage::operand_);
			if(registers.pc.halves.high == Storage::address_.halves.high) {
				goto fetch_decode;
			}

			Storage::address_.halves.low = registers.pc.halves.low;
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);

			goto fetch_decode;

		// MARK: - Zero.

		case access_program(Zero):
			++registers.pc.full;
			Storage::address_.halves.low = Storage::operand_;
			goto access_zero;

		// MARK: - Zero indexed.

		case access_program(ZeroIndexed):
			++registers.pc.full;

			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, ZeroPage(Storage::operand_), throwaway);
			Storage::address_.halves.low += index();

			goto access_zero;


		// MARK: - Absolute.

		case access_program(Absolute):
			++registers.pc.full;

			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			++registers.pc.full;

			goto access_absolute;

		// MARK: - Absolute indexed.

		case access_program(AbsoluteIndexed):
			++registers.pc.full;

			// Read top half of address.
			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			++registers.pc.full;

			// If this is a read and the top byte doesn't need adjusting, skip that cycle.
			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += index();
			if(Storage::decoded_.type == Type::Read && Storage::operand_ == Storage::address_.halves.high) {
				goto access_absolute;
			}

			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			std::swap(Storage::address_.halves.high, Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);
			std::swap(Storage::address_.halves.high, Storage::operand_);
			goto access_absolute;

		// MARK: - Indexed indirect.

		case access_program(IndexedIndirect):
			++registers.pc.full;

			access(BusOperation::Read, ZeroPage(Storage::operand_), throwaway);
			Storage::operand_ += registers.x;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low);
			++Storage::operand_;
			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);
			goto access_absolute;

		// MARK: - Indirect indexed.

		case access_program(IndirectIndexed):
			++registers.pc.full;

			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low);
			++Storage::operand_;

			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);

			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += registers.y;
			if(Storage::decoded_.type == Type::Read && Storage::address_.halves.high == Storage::operand_) {
				goto access_absolute;
			}

			if constexpr (is_65c02(model)) {
				check_interrupt();
			}
			std::swap(Storage::address_.halves.high, Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);
			std::swap(Storage::address_.halves.high, Storage::operand_);
			goto access_absolute;

		// MARK: - Potentially-faulty addressing of SHA/SHX/SHY/SHS.

		case access_program(SHxAbsoluteXY):
			++registers.pc.full;

			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			++registers.pc.full;

			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += (Storage::decoded_.operation == Operation::SHY) ? registers.x : registers.y;
			Storage::did_adjust_top_ = Storage::address_.halves.high != Storage::operand_;

			std::swap(Storage::address_.halves.high, Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);
			std::swap(Storage::address_.halves.high, Storage::operand_);

			switch(Storage::decoded_.operation) {
				default: __builtin_unreachable();
				case Operation::SHA:
					Operations::sha(registers, Storage::address_, Storage::operand_, Storage::did_adjust_top_);
				break;
				case Operation::SHX:
					Operations::shx(registers, Storage::address_, Storage::operand_, Storage::did_adjust_top_);
				break;
				case Operation::SHY:
					Operations::shy(registers, Storage::address_, Storage::operand_, Storage::did_adjust_top_);
				break;
				case Operation::SHS:
					Operations::shs(registers, Storage::address_, Storage::operand_, Storage::did_adjust_top_);
				break;
			}

			check_interrupt();
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);

			goto fetch_decode;

		case access_program(SHxIndirectIndexed):
			++registers.pc.full;

			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low);
			++Storage::operand_;

			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);

			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += registers.y;
			Storage::did_adjust_top_ = Storage::address_.halves.high != Storage::operand_;

			std::swap(Storage::address_.halves.high, Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);
			std::swap(Storage::address_.halves.high, Storage::operand_);

			check_interrupt();
			assert(Storage::decoded_.operation == Operation::SHA);
			Operations::sha(registers, Storage::address_, Storage::operand_, Storage::did_adjust_top_);
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

		// MARK: - Flow control (other than BRK).

		case access_program(JSR):
			++registers.pc.full;
			access(BusOperation::Read, Stack(registers.s), throwaway);
			access(BusOperation::Write, Stack(registers.dec_s()), registers.pc.halves.high);
			access(BusOperation::Write, Stack(registers.dec_s()), registers.pc.halves.low);

			check_interrupt();
			access(BusOperation::Read, Literal(registers.pc.full), registers.pc.halves.high);
			registers.pc.halves.low = Storage::operand_;

			goto fetch_decode;

		case access_program(RTI):
			access(BusOperation::Read, Stack(registers.s), Storage::operand_);

			access(BusOperation::Read, Stack(registers.inc_s()), Storage::operand_);
			registers.flags = Flags(Storage::operand_);

			access(BusOperation::Read, Stack(registers.inc_s()), registers.pc.halves.low);
			check_interrupt();
			access(BusOperation::Read, Stack(registers.inc_s()), registers.pc.halves.high);

			goto fetch_decode;

		case access_program(RTS):
			access(BusOperation::Read, Stack(registers.s), Storage::operand_);

			access(BusOperation::Read, Stack(registers.inc_s()), registers.pc.halves.low);
			access(BusOperation::Read, Stack(registers.inc_s()), registers.pc.halves.high);

			check_interrupt();
			access(BusOperation::Read, Literal(registers.pc.full), throwaway);
			++registers.pc.full;

			goto fetch_decode;

		case access_program(JMPAbsolute):
			++registers.pc.full;
			check_interrupt();
			access(BusOperation::Read, Literal(registers.pc.full), registers.pc.halves.high);
			registers.pc.halves.low = Storage::operand_;

			goto fetch_decode;

		case access_program(JMPAbsoluteIndirect):
			++registers.pc.full;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			Storage::address_.halves.low = Storage::operand_;

			access(BusOperation::Read, Literal(Storage::address_.full), registers.pc.halves.low);
			++Storage::address_.halves.low;
			check_interrupt();
			access(BusOperation::Read, Literal(Storage::address_.full), registers.pc.halves.high);

			goto fetch_decode;

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

	#undef access_program
	#undef access
	#undef access_label
	#undef attach
	#undef join
	#undef restore_point
}

}
