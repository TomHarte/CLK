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
	Write
};

constexpr bool is_read(const ReadWrite read_write) {
	return read_write == ReadWrite::Read;
}
constexpr Bus::Data::AccessType access_type(const ReadWrite read_write) {
	switch(read_write) {
		case ReadWrite::Read: return Bus::Data::AccessType::Read;
		case ReadWrite::Write: return Bus::Data::AccessType::Write;
	}
}

enum class Line {
	Halt,
	DMABusReq,
	NMI,
	IRQ,
	FIRQ,
	MRDY,
};

// Missing inputs:
//
//	MRDY — allows the bus to be stretched in 1/4 cycle increments
//
// Missing outputs:
//
//	LIC (6809E) - high during the last cycle of any instruction; hence high -> low indicates fetch beginning.
//	AVMA — advanced VMA, i.e. bus will be accessed in the next cycle
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
	BetweenInstructions,
	AnyCycle,
};

// MARK: - Processor implementation.

template <typename Traits>
struct Processor {
	Processor(Traits::BusHandlerT &bus_handler) noexcept : bus_handler_(bus_handler) {}

	void run_for(const Cycles cycles) {
		static constexpr auto FirstCounter = __COUNTER__;
		#define restore_point()	(__COUNTER__ - FirstCounter + int(ResumePoint::Max) + int(AddressingMode::Max))

		#define join(a, b)			a##b
		#define attach(a, b)		join(a, b)
		#define access_label()		attach(repeat, __LINE__)

		#define access(read_write, bus_state, addr, value, ...)	{									\
			static constexpr int location = restore_point();										\
			[[fallthrough]]; case location:															\
			[[maybe_unused]] access_label():														\
																									\
			if constexpr (Traits::pause_precision >= PausePrecision::AnyCycle) {					\
				if(cycles_ <= Cycles(0)) {															\
					resume_point_ = location;														\
					return;																			\
				}																					\
			}																						\
																									\
			if constexpr (is_read(read_write)) {																	\
				if constexpr (std::is_same_v<decltype(value), Data::Writeable>) {							\
					cycles_ -= bus_handler_.template perform<read_write, bus_state>(addr, value);			\
				} else {																					\
					Data::Writeable target;																	\
					cycles_ -= bus_handler_.template perform<read_write, bus_state>(addr, target);			\
					value = target;													\
				}																							\
			} else {																						\
				cycles_ -= bus_handler_.template perform<read_write, bus_state>(addr, value);				\
			}																								\
			__VA_ARGS__;																			\
		}

		#define access_program(name)	int(ResumePoint::Max) + int(AddressingMode::name)

		cycles_ += cycles;

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
				if(cycles_ <= Cycles(0)) {
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
	Cycles cycles_;

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
