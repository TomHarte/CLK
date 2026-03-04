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
#include "InstructionSets/6809/OperationMapper.hpp"

#include "Processors/Bus/PartialAddresses.hpp"
#include "Processors/Bus/Data.hpp"

namespace CPU::M6809 {

// MARK: - Processor bus signalling.

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

// MARK: - Data bus.

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

		cycles_ += cycles;

		uint8_t opcode_;
		while(true) switch(resume_point_) {
			default:
				__builtin_unreachable();

			// MARK: - Fetch/decode.

//			fetch_decode:
			case ResumePoint::FetchDecode:
				access(ReadWrite::Read, BusState::Normal, registers_.pc, opcode_);
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
	Registers registers_;
};

}
