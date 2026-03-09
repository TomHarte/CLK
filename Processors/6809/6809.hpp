//
//  6809.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Registers.hpp"
#include "ClockReceiver/ClockReceiver.hpp"

#include "Reflection/Dispatcher.hpp"
#include "InstructionSets/6809/OperationMapper.hpp"

#include "Processors/Bus/PartialAddresses.hpp"
#include "Processors/Bus/Data.hpp"

#include <type_traits>

namespace CPU::M6809 {

// MARK: - Processor bus signalling.

// Defined as BA:BS.
enum class BusState {
	Normal = 0b00,
	InterruptOrResetAcknowledge = 0b01,
	SyncAcknowledge = 0b10,
	HaltOrBusGrantAcknowledge = 0b11,
};

enum class ReadWrite {
	Read,
	ReadLast,	// Read with LIC high.
	Write,
	WriteLast	// Write with LIC high.,
};
constexpr bool is_read(const ReadWrite read_write) {
	return read_write < ReadWrite::Write;
}
constexpr Bus::Data::AccessType access_type(const ReadWrite read_write) {
	if(is_read(read_write)) {
		return Bus::Data::AccessType::Read;
	} else {
		return Bus::Data::AccessType::Write;
	}
}

enum class BusPhase {
	PreMRDY,
	MRDY,
	PostMRDY,
	NonAccess,
};
constexpr bool is_terminal(const BusPhase phase) {
	return phase >= BusPhase::PostMRDY;
}

enum class Line {
	Halt,
	DMABusReq,
	NMI,
	IRQ,
	FIRQ,
	MRDY,	// Allows the bus to be stretched in 1/4 cycle increments (on a non-E 6809).
};

// Missing outputs:
//
//	AVMA — advanced VMA, i.e. bus will be accessed in the next cycle.
//
// clocks: (Q, E, TSC, XTAL, EXTAL)

// MARK: - Bus.

namespace Address {

using Literal = Bus::Address::Literal<uint16_t>;

}

namespace Data {

using Writeable = Bus::Data::Writeable<uint8_t>;
using NoValue = Bus::Data::NoValue<uint8_t>;

}

template <ReadWrite read_write>
using data_t = Bus::Data::data_t<uint8_t, access_type(read_write)>;

// MARK: - Code-generation choices.

enum class PausePrecision {
	/// The 6809 will potentially exit only at the start of each distinct instruction.
	BetweenInstructions,
	/// The 6809 will potentially exit before beginning any bus access.
	BetweenAccesses,
	/// The 6809 will potentially exit before beginning any call to the bus handler; if MRDY is in use then this is more granular than
	/// between accesses — the processor may end its current run during an MRDY pause.
	BetweenBusActions,
};

// MARK: - Processor implementation.

template <typename Traits>
struct Processor {
	// Time getter.
	using Timescale = std::conditional_t<Traits::uses_mrdy, QuarterCycles, Cycles>;
	static constexpr Timescale duration([[maybe_unused]] const BusPhase phase) {
		if constexpr (std::is_same_v<Timescale, Cycles>) {
			return Cycles(1);
		} else {
			switch(phase) {
				case BusPhase::PreMRDY:		return QuarterCycles(3);
				case BusPhase::MRDY:		return QuarterCycles(1);
				case BusPhase::PostMRDY:	return QuarterCycles(1);
				case BusPhase::NonAccess:	return QuarterCycles(4);
				default: __builtin_unreachable();
			}
		}
	}

	Processor(Traits::BusHandlerT &bus_handler) noexcept : bus_handler_(bus_handler) {}

	void run_for(const Timescale duration) {
		static constexpr auto FirstCounter = __COUNTER__;
		#define restore_point()	(__COUNTER__ - FirstCounter + int(ResumePoint::Max) + int(AddressingMode::Max))

		#define join(a, b)			a##b
		#define attach(a, b)		join(a, b)
		#define access_label()		attach(repeat, __LINE__)

		#define access(read_write, bus_state, addr, value, ...)	{											\
			static constexpr int location = restore_point();												\
			[[fallthrough]]; case location:																	\
			[[maybe_unused]] access_label():																\
																											\
			if constexpr (Traits::pause_precision >= PausePrecision::BetweenAccesses) {						\
				if(time_ <= 0) {																			\
					resume_point_ = location;																\
					return;																					\
				}																							\
			}																								\
																											\
			if constexpr (is_read(read_write)) {															\
				if constexpr (std::is_same_v<decltype(value), Data::Writeable>) {							\
					time_ -= bus_handler_.template perform<read_write, bus_state>(addr, value);			\
				} else {																					\
					Data::Writeable target;																	\
					time_ -= bus_handler_.template perform<read_write, bus_state>(addr, target);			\
					value = target;																			\
				}																							\
			} else {																						\
				time_ -= bus_handler_.template perform<read_write, bus_state>(addr, value);				\
			}																								\
			__VA_ARGS__;																					\
		}

		#define access_program(name)	int(ResumePoint::Max) + int(AddressingMode::name)

		time_ += duration;

		using Literal = Address::Literal;
		using Operation = InstructionSet::M6809::Operation;

		InstructionSet::M6809::OperationReturner op_returner;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page0> op_mapper0;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page1> op_mapper1;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page2> op_mapper2;

		uint8_t opcode;
		while(true) switch(resume_point_) {
			default:
				__builtin_unreachable();

			// MARK: - Fetch/decode.

//			fetch_decode:
			case ResumePoint::FetchDecode:
				if(time_ <= 0) {
					resume_point_ = ResumePoint::FetchDecode;
					return;
				}

				// TODO: branch out here if interrupts or exceptions exist.

				access(ReadWrite::Read, BusState::Normal, Literal(registers_.pc), opcode, ++registers_.pc);
				{
					const auto decoding = Reflection::dispatch(op_mapper0, opcode, op_returner);
					operation_ = decoding.first;
					resume_point_ = ResumePoint::Max + int(decoding.second);
					break;
				}

			fetch_decode_page1:
				access(ReadWrite::Read, BusState::Normal, Literal(registers_.pc), opcode, ++registers_.pc);
				{
					const auto decoding = Reflection::dispatch(op_mapper1, opcode, op_returner);
					operation_ = decoding.first;
					resume_point_ = ResumePoint::Max + int(decoding.second);
					break;
				}

			fetch_decode_page2:
				access(ReadWrite::Read, BusState::Normal, Literal(registers_.pc), opcode, ++registers_.pc);
				{
					const auto decoding = Reflection::dispatch(op_mapper2, opcode, op_returner);
					operation_ = decoding.first;
					resume_point_ = ResumePoint::Max + int(decoding.second);
					break;
				}

			case access_program(Variant):
				if(operation_ == Operation::Page2) {
					goto fetch_decode_page2;
				} else {
					goto fetch_decode_page1;
				}
		}


		#undef access_label
		#undef attach
		#undef join
		#undef access
		#undef restore_point
	}

private:
	Traits::BusHandlerT &bus_handler_;
	Timescale time_;

	enum ResumePoint {
		FetchDecode,
		Max,
	};
	using AddressingMode = InstructionSet::M6809::AddressingMode;

	int resume_point_;
	InstructionSet::M6809::Operation operation_;
	Registers registers_;
};

}
