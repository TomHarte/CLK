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
			cycles_ -= bus_handler_.template perform<read_write, bus_state>(addr, value);			\
			__VA_ARGS__;																			\
		}

		cycles_ += cycles;

		uint8_t operation_;
		while(true) switch(resume_point_) {
			default:
				__builtin_unreachable();

			// MARK: - Fetch/decode.

			fetch_decode:
			case ResumePoint::FetchDecode:
				access(ReadWrite::Read, BusState::Normal, registers_.pc, operation_);
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
