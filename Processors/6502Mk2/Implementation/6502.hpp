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

// On the 65c02: http://www.6502.org/tutorials/65c02opcodes.html
// Some bus captures to substantiate 65c02 timing:
//		https://github.com/CompuSAR/sar6502/blob/master/sar6502.srcs/sim_1/new/test_plan.mem

namespace CPU::MOS6502Mk2 {

template <Model model, typename Traits>
void Processor<model, Traits>::restart_operation_fetch() {
	Storage::resume_point_ = Storage::ResumePoint::FetchDecode;
	Storage::cycles_ = Cycles(0);
}

template <Model model, typename Traits>
void Processor<model, Traits>::run_for(const Cycles cycles) {
	using ResumePoint = Storage::ResumePoint;
	using InterruptRequest = Storage::Inputs::InterruptRequest;
	auto &registers = Storage::registers_;

	Storage::cycles_ += cycles;
	if(Storage::cycles_ <= Cycles(0)) return;

	struct WriteableReader {
		static void assign(uint8_t &lhs, const Data::Writeable rhs) {
			lhs = rhs;
		}
		static void assign(const uint8_t &, const Data::Writeable) {}
	};
	const auto check_interrupt = [&] {
		Storage::captured_interrupt_requests_ =
			Storage::inputs_.interrupt_requests &
				(Storage::registers_.flags.inverse_interrupt | ~InterruptRequest::IRQ);
	};

	#define restore_point()	(__COUNTER__ + int(ResumePoint::Max) + int(AddressingMode::Max))

	#define join(a, b)			a##b
	#define attach(a, b)		join(a, b)
	#define access_label()		attach(repeat, __LINE__)

	// TODO: find a way not to generate a restore point if pause precision and uses_ready_line/model allows it.
	#define access(type, addr, value, ...)	{															\
		static constexpr int location = restore_point();												\
		[[fallthrough]]; case location:																	\
		[[maybe_unused]] access_label():																\
		check_interrupt();																				\
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
		if constexpr (is_read(type)) {																	\
			if constexpr (std::is_same_v<decltype(value), Data::Writeable>) {							\
				Storage::cycles_ -= Storage::bus_handler_.template perform<type>(addr, value);			\
			} else {																					\
				Data::Writeable target;																	\
				Storage::cycles_ -= Storage::bus_handler_.template perform<type>(addr, target);			\
				WriteableReader::assign(value, target);													\
			}																							\
		} else {																						\
			Storage::cycles_ -= Storage::bus_handler_.template perform<type>(addr, value);				\
		}																								\
		__VA_ARGS__;																					\
	}

	#define access_program(name)	int(ResumePoint::Max) + int(AddressingMode::name)

	Data::Writeable throwaway;

	const auto index = [&] {
		return Storage::decoded_.index == Index::X ? registers.x : registers.y;
	};
	const auto perform_operation = [&] {
		CPU::MOS6502Mk2::perform<model>(
			Storage::decoded_.operation,
			registers,
			Storage::operand_,
			Storage::opcode_
		);
	};
	const auto needs_65c02_extra_arithmetic_cycle = [&] {
		return
			is_65c02(model) &&
			(
				Storage::decoded_.operation == Operation::ADC ||
				Storage::decoded_.operation == Operation::SBC
			) && registers.flags.decimal;
	};
	const auto set_interrupt_flag = [&] {
		registers.flags.inverse_interrupt = 0;
		if constexpr (is_65c02(model)) {
			registers.flags.decimal = 0;
		}
	};

	using Literal = Address::Literal;
	using ZeroPage = Address::ZeroPage;
	using Stack = Address::Stack;
	using Vector = Address::Vector;

	while(true) switch(Storage::resume_point_) {
		default:
			__builtin_unreachable();

		// MARK: - Read, write or modify a zero-page address.

		access_zero:
			++registers.pc.full;
			if constexpr (is_65c02(model)) {
				if(Storage::decoded_.operation == Operation::FastNOP) {
					goto fetch_decode;
				}
			}

			if(Storage::decoded_.type == Type::Write) {
				goto access_zero_write;
			}

			// ADC and SBC decimal take an extra cycle on the 65c02.
			if(needs_65c02_extra_arithmetic_cycle()) {
				goto access_zero_65c02_decimal;
			}

			// Read.
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
			perform_operation();
			access(BusOperation::Write, ZeroPage(Storage::address_.halves.low), Storage::operand_);

			goto fetch_decode;

		access_zero_65c02_decimal:
			access(BusOperation::Read, ZeroPage(Storage::address_.halves.low), Storage::operand_);
			access(BusOperation::Read, ZeroPage(Storage::address_.halves.low), Storage::operand_);
			perform_operation();
			goto fetch_decode;

		// MARK: - Read, write or modify an arbitrary address.

		access_absolute:
			++registers.pc.full;
			if constexpr (is_65c02(model)) {
				if(Storage::decoded_.operation == Operation::FastNOP) {
					goto fetch_decode;
				}
			}
			if(Storage::decoded_.type == Type::Write) {
				goto access_absolute_write;
			}

			// ADC and SBC decimal take an extra cycle on the 65c02.
			if(needs_65c02_extra_arithmetic_cycle()) {
				goto access_absolute_65c02_decimal;
			}

			// Read.
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
			perform_operation();
			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);

			goto fetch_decode;

		access_absolute_65c02_decimal:
			access(BusOperation::Read, Literal(Storage::address_.full), Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), Storage::operand_);
			perform_operation();
			goto fetch_decode;

		// MARK: - Fetch/decode.

		fetch_decode:
		case ResumePoint::FetchDecode:

			// Pause precision will always be at least operation by operation.
			if(Storage::cycles_ <= Cycles(0)) {
				Storage::resume_point_ = ResumePoint::FetchDecode;
				return;
			}

			if(Storage::captured_interrupt_requests_) {
				goto interrupt;
			}

			access(BusOperation::ReadOpcode, Literal(registers.pc.full), Storage::opcode_, ++registers.pc.full);
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

			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_);

			Storage::resume_point_ = ResumePoint::Max + int(Storage::decoded_.mode);
			break;

		// MARK: - Immediate, Implied, Accumulator.

		case access_program(Immediate):
			if(needs_65c02_extra_arithmetic_cycle()) {
				goto immediate_65c02_decimal;
			}
			++registers.pc.full;
			[[fallthrough]];

		case access_program(Implied):
			perform_operation();
			goto fetch_decode;

		immediate_65c02_decimal:
			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_, ++registers.pc.full);
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
			access(BusOperation::Read, Stack(registers.s), Storage::operand_, ++registers.s);
			access(BusOperation::Read, Stack(registers.s), Storage::operand_);
			perform_operation();
			goto fetch_decode;

		case access_program(Push):
			perform_operation();
			access(BusOperation::Write, Stack(registers.s), Storage::operand_, --registers.s);
			goto fetch_decode;

		// MARK: - Relative, and BBR/BBS (for the 65c02).

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

		case access_program(BBRBBS):
			++registers.pc.full;
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, ZeroPage(Storage::address_.halves.low), Storage::operand_);
			access(BusOperation::Read, ZeroPage(Storage::address_.halves.low), Storage::operand_);
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.low, ++registers.pc.full);

			if(!test_bbr_bbs(Storage::opcode_, Storage::operand_)) {
				goto fetch_decode;
			}

			Storage::operand_ = Storage::address_.halves.low;
			Storage::address_ = registers.pc;
			access(BusOperation::Read, Literal(registers.pc.full), throwaway);

			registers.pc.full += int8_t(Storage::operand_);
			if(registers.pc.halves.high == Storage::address_.halves.high) {
				goto fetch_decode;
			}

			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);

			goto fetch_decode;

		// MARK: - Zero.

		case access_program(Zero):
			Storage::address_.halves.low = Storage::operand_;
			goto access_zero;

		// MARK: - Zero indexed.

		case access_program(ZeroIndexed):
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, ZeroPage(Storage::operand_), throwaway);
			Storage::address_.halves.low += index();

			goto access_zero;

		// MARK: - Zero indirect (which is exclusive to the 65c02).

		case access_program(ZeroIndirect):
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low, ++Storage::operand_);
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);

			goto access_absolute;

		// MARK: - Absolute.

		case access_program(Absolute):
			++registers.pc.full;
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);

			goto access_absolute;

		// MARK: - Absolute indexed.

		case access_program(AbsoluteIndexed):
			++registers.pc.full;

			// Read top half of address.
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);

			// If this is a read and the top byte doesn't need adjusting, skip that cycle.
			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += index();
			if(Storage::decoded_.type == Type::Read && Storage::operand_ == Storage::address_.halves.high) {
				goto access_absolute;
			}

			if constexpr (is_65c02(model)) {
				goto absolute_indexed_65c02_tail;
			}
			std::swap(Storage::address_.halves.high, Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);
			std::swap(Storage::address_.halves.high, Storage::operand_);
			goto access_absolute;

		absolute_indexed_65c02_tail:
			access(BusOperation::Read, Literal(registers.pc.full), throwaway);
			goto access_absolute;

		// MARK: - Fast absolute indexed modify, which is a 65c02 improvement but not applied universally.

		case access_program(FastAbsoluteIndexedModify):
			++registers.pc.full;

			// Read top half of address.
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);

			// If this is a read and the top byte doesn't need adjusting, skip that cycle.
			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += index();
			if(Storage::address_.halves.high == Storage::operand_) {
				goto access_absolute;
			}

			access(BusOperation::Read, Literal(registers.pc.full), throwaway);
			goto access_absolute;

		// MARK: - Indexed indirect.

		case access_program(IndexedIndirect):
			access(BusOperation::Read, ZeroPage(Storage::operand_), throwaway);
			Storage::operand_ += registers.x;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low, ++Storage::operand_);
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);
			goto access_absolute;

		// MARK: - Indirect indexed.

		case access_program(IndirectIndexed):
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low, ++Storage::operand_);
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);

			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += registers.y;
			if(Storage::decoded_.type == Type::Read && Storage::address_.halves.high == Storage::operand_) {
				goto access_absolute;
			}

			if constexpr (is_65c02(model)) {
				goto indirect_indexed_65c02_tail;
			}

			std::swap(Storage::address_.halves.high, Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);
			std::swap(Storage::address_.halves.high, Storage::operand_);

			goto access_absolute;

		indirect_indexed_65c02_tail:
			access(BusOperation::Read, Literal(registers.pc.full), throwaway);
			goto access_absolute;

		// MARK: - Potentially-faulty addressing of SHA/SHX/SHY/SHS.

		case access_program(SHxAbsoluteXY):
			++registers.pc.full;
			Storage::address_.halves.low = Storage::operand_;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high, ++registers.pc.full);

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

			access(BusOperation::Write, Literal(Storage::address_.full), Storage::operand_);

			goto fetch_decode;

		case access_program(SHxIndirectIndexed):
			++registers.pc.full;
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.low, ++Storage::operand_);
			access(BusOperation::Read, ZeroPage(Storage::operand_), Storage::address_.halves.high);

			Storage::operand_ = Storage::address_.halves.high;
			Storage::address_.full += registers.y;
			Storage::did_adjust_top_ = Storage::address_.halves.high != Storage::operand_;

			std::swap(Storage::address_.halves.high, Storage::operand_);
			access(BusOperation::Read, Literal(Storage::address_.full), throwaway);
			std::swap(Storage::address_.halves.high, Storage::operand_);

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
			access(BusOperation::Write, Stack(registers.s), registers.pc.halves.high, --registers.s);
			access(BusOperation::Write, Stack(registers.s), registers.pc.halves.low, --registers.s);
			access(BusOperation::Read, Literal(registers.pc.full), registers.pc.halves.high);
			registers.pc.halves.low = Storage::operand_;

			goto fetch_decode;

		case access_program(RTI):
			access(BusOperation::Read, Stack(registers.s), Storage::operand_, ++registers.s);

			access(BusOperation::Read, Stack(registers.s), Storage::operand_,  ++registers.s);
			registers.flags = Flags(Storage::operand_);

			access(BusOperation::Read, Stack(registers.s), registers.pc.halves.low,  ++registers.s);
			access(BusOperation::Read, Stack(registers.s), registers.pc.halves.high);

			goto fetch_decode;

		case access_program(RTS):
			access(BusOperation::Read, Stack(registers.s), Storage::operand_, ++registers.s);

			access(BusOperation::Read, Stack(registers.s), registers.pc.halves.low, ++registers.s);
			access(BusOperation::Read, Stack(registers.s), registers.pc.halves.high);
			access(BusOperation::Read, Literal(registers.pc.full), throwaway, ++registers.pc.full);

			goto fetch_decode;

		case access_program(JMPAbsolute):
			++registers.pc.full;
			access(BusOperation::Read, Literal(registers.pc.full), registers.pc.halves.high);
			registers.pc.halves.low = Storage::operand_;

			goto fetch_decode;

		case access_program(JMPAbsoluteIndirect):
			++registers.pc.full;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			Storage::address_.halves.low = Storage::operand_;

			access(BusOperation::Read, Literal(Storage::address_.full), registers.pc.halves.low);
			++Storage::address_.halves.low;
			access(BusOperation::Read, Literal(Storage::address_.full), registers.pc.halves.high);

			if constexpr (!is_65c02(model)) {
				goto fetch_decode;
			}

			Storage::address_.halves.high += !Storage::address_.halves.low;
			access(BusOperation::Read, Literal(Storage::address_.full), registers.pc.halves.high);

			goto fetch_decode;

		case access_program(JMPAbsoluteIndexedIndirect):
			++registers.pc.full;
			access(BusOperation::Read, Literal(registers.pc.full), Storage::address_.halves.high);
			Storage::address_.halves.low = Storage::operand_;

			access(BusOperation::Read, Literal(registers.pc.full), throwaway);

			Storage::address_.full += registers.x;
			access(
				BusOperation::Read,
				Literal(Storage::address_.full),
				registers.pc.halves.low,
				++Storage::address_.full
			);
			access(BusOperation::Read, Literal(Storage::address_.full), registers.pc.halves.high);

			goto fetch_decode;

		// MARK: - NMI/IRQ/Reset, and BRK.

		case access_program(BRK):
			++registers.pc.full;
			access(BusOperation::Write, Stack(registers.s), registers.pc.halves.high, --registers.s);
			access(BusOperation::Write, Stack(registers.s), registers.pc.halves.low, --registers.s);
			access(
				BusOperation::Write,
				Stack(registers.s),
				static_cast<uint8_t>(registers.flags) | Flag::Break,
				--registers.s
			);

			set_interrupt_flag();
			access(BusOperation::Read, Vector(0xfe), registers.pc.halves.low);
			access(BusOperation::Read, Vector(0xff), registers.pc.halves.high);
			goto fetch_decode;

		interrupt:
			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_);
			access(BusOperation::Read, Literal(registers.pc.full), Storage::operand_);

			if(Storage::captured_interrupt_requests_ & (InterruptRequest::Reset | InterruptRequest::PowerOn)) {
				Storage::inputs_.interrupt_requests &= ~InterruptRequest::PowerOn;
				goto reset;
			}

			access(BusOperation::Write, Stack(registers.s), registers.pc.halves.high, --registers.s);
			access(BusOperation::Write, Stack(registers.s), registers.pc.halves.low, --registers.s);
			access(
				BusOperation::Write,
				Stack(registers.s),
				static_cast<uint8_t>(registers.flags) & ~Flag::Break,
				--registers.s
			);

			set_interrupt_flag();
			if(Storage::captured_interrupt_requests_ & InterruptRequest::NMI) {
				Storage::inputs_.interrupt_requests &= ~InterruptRequest::NMI;
				goto nmi;
			}

			access(BusOperation::Read, Vector(0xfe), registers.pc.halves.low);
			access(BusOperation::Read, Vector(0xff), registers.pc.halves.high);
			goto fetch_decode;

		nmi:
			access(BusOperation::Read, Vector(0xfa), registers.pc.halves.low);
			access(BusOperation::Read, Vector(0xfb), registers.pc.halves.high);
			goto fetch_decode;

		reset:
			access(BusOperation::Read, Stack(registers.s), Storage::operand_, --registers.s);
			access(BusOperation::Read, Stack(registers.s), Storage::operand_, --registers.s);
			access(BusOperation::Read, Stack(registers.s), Storage::operand_, --registers.s);

			set_interrupt_flag();
			access(BusOperation::Read, Vector(0xfc), registers.pc.halves.low);
			access(BusOperation::Read, Vector(0xfd), registers.pc.halves.high);
			goto fetch_decode;

		// MARK: - STP and WAI.
		case access_program(STP):
		stopped:
			if(Storage::cycles_ <= Cycles(0)) {
				Storage::resume_point_ = access_program(STP);
				return;
			}
			access(BusOperation::None, Vector(0xff), Data::NoValue{});
			if(Storage::captured_interrupt_requests_ & (InterruptRequest::Reset | InterruptRequest::PowerOn)) {
				goto fetch_decode;
			}
			goto stopped;

		case access_program(WAI):
		waiting:
			if(Storage::cycles_ <= Cycles(0)) {
				Storage::resume_point_ = access_program(WAI);
				return;
			}
			access(BusOperation::Ready, Vector(0xff), Data::NoValue{});
			if(Storage::captured_interrupt_requests_) {
				goto fetch_decode;
			}
			goto waiting;
	}

	#undef access_program
	#undef access
	#undef access_label
	#undef attach
	#undef join
	#undef restore_point
}

}
