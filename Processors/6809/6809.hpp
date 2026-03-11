//
//  6809.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Perform.hpp"
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
constexpr bool is_write(const ReadWrite read_write) {
	return read_write >= ReadWrite::Write;
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
	FullCycle,
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
template <uint16_t address>
using Fixed = Bus::Address::Fixed<uint16_t, address>;

}

namespace Data {

using Writeable = Bus::Data::Writeable<uint8_t, false>;
using NoValue = Bus::Data::NoValue<uint8_t>;

}

template <ReadWrite read_write>
using data_t = Bus::Data::data_t<uint8_t, false, access_type(read_write)>;

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
			// My reading of the data sheet is that if accessing memory then in each EQ quadrants
			// the 6809 proceeds as:
			//
			// EQ: 00
			// EQ: 01		address becomes valid
			// EQ: 11		data transfer begins
			// ...			waiting if MRDY requested
			// EQ: 10
			//
			// So that splits each access into 3/4 of a cycle in which the state of MRDY doesn't matter,
			// arbitrarily more quarters of a cycle in which the processor pauses if MRDY is active,
			// then a final quarter of a cycle to complete the access.
			//
			// Code expectation: some machines won't use MRDY and therefore won't be interested in bookkeeping at
			// quarter-cycle precision. In that case all machine cycles last one cycle long, as per the if constexpr
			// above. Otherwise machines can take the BusPhase handed to them and enquire as to length.
			switch(phase) {
				case BusPhase::PreMRDY:		return QuarterCycles(3);
				case BusPhase::MRDY:		return QuarterCycles(1);
				case BusPhase::PostMRDY:	return QuarterCycles(1);
				case BusPhase::FullCycle:	return QuarterCycles(4);
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
		#define local_label(name)	attach(name, __LINE__)
		#define access_label()		local_label(repeat)

		#define check_pause(precision, location)						\
			if constexpr (Traits::pause_precision >= precision) {		\
				if(time_ <= 0) {										\
					resume_point_ = location;							\
					return;												\
				}														\
			}

		#define read(bus_state, addr, value, ...) {																	\
			if constexpr (!Traits::uses_mrdy) {																		\
				time_ -= Cycles(1);																					\
				time_ -= 																							\
					bus_handler_.template perform<BusPhase::FullCycle, ReadWrite::Read, bus_state>(addr, target_);	\
				goto local_label(label_prefix##SkipMRDY);															\
			}																										\
																													\
			if constexpr (Traits::uses_mrdy) {																		\
				time_ -= QuarterCycles(3);																			\
				time_ -=																							\
					bus_handler_.template perform<BusPhase::PreMRDY, ReadWrite::Read, bus_state>(addr, target_);	\
			}																										\
																													\
			static constexpr auto check_mrdy = restore_point();														\
			[[fallthrough]];																						\
			case check_mrdy:																						\
			while(mrdy_) {																							\
				check_pause(PausePrecision::BetweenBusActions, check_mrdy);											\
				if constexpr (Traits::uses_mrdy) {																	\
					time_ -= QuarterCycles(1);																		\
					time_ -=																						\
						bus_handler_.template perform<BusPhase::MRDY, ReadWrite::Read, bus_state>(addr, target_);	\
				}																									\
			}																										\
																													\
			static constexpr auto post_mrdy = restore_point();														\
			[[fallthrough]];																						\
			case post_mrdy:																							\
			check_pause(PausePrecision::BetweenBusActions, post_mrdy);												\
																													\
			if constexpr (Traits::uses_mrdy) {																		\
				time_ -= QuarterCycles(1);																			\
				time_ -=																							\
					bus_handler_.template perform<BusPhase::PostMRDY, ReadWrite::Read, bus_state>(addr, target_);	\
			}																										\
																													\
			local_label(label_prefix##SkipMRDY):																	\
			value = target_;																						\
			__VA_ARGS__;																							\
		}

		#define access_program(name)	int(ResumePoint::Max) + int(AddressingMode::name)

		time_ += duration;

		using Literal = Address::Literal;
		using Operation = InstructionSet::M6809::Operation;

		InstructionSet::M6809::OperationReturner op_returner;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page0> op_mapper0;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page1> op_mapper1;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page2> op_mapper2;

		const auto perform = [&]() {
			CPU::M6809::perform(operation_, registers_, operand_);
		};

		uint8_t opcode = 0;
		while(true) switch(resume_point_) {
			default:
				__builtin_unreachable();

			// MARK: - Exceptions.

			reset:
				registers_.dp = 0;
				exceptions_ &= ~(Exceptions::NMI | Exceptions::PowerOnReset);

				// TODO: spin here on HALT | DMA | BREQ.

				read(BusState::InterruptOrResetAcknowledge, Address::Fixed<0xfffe>(), registers_.pc.halves.high);
				read(BusState::InterruptOrResetAcknowledge, Address::Fixed<0xffff>(), registers_.pc.halves.low);

				goto fetch_decode;


			// MARK: - Fetch/decode.

			fetch_decode:
			case ResumePoint::FetchDecode:
				if(time_ <= 0) {
					resume_point_ = ResumePoint::FetchDecode;
					return;
				}

				if(exceptions_) {
					if(exceptions_ & (Exceptions::PowerOnReset | Exceptions::Reset)) {
						goto reset;
					}
					// TODO: test interrupts and more.
				}

				read(BusState::Normal, Literal(registers_.pc.full), opcode, ++registers_.pc.full);
				{
					const auto decoding = Reflection::dispatch(op_mapper0, opcode, op_returner);
					operation_ = decoding.first;
					resume_point_ = ResumePoint::Max + int(decoding.second);
					break;
				}

			fetch_decode_page1:
				read(BusState::Normal, Literal(registers_.pc.full), opcode, ++registers_.pc.full);
				{
					const auto decoding = Reflection::dispatch(op_mapper1, opcode, op_returner);
					operation_ = decoding.first;
					resume_point_ = ResumePoint::Max + int(decoding.second);
					break;
				}

			fetch_decode_page2:
				read(BusState::Normal, Literal(registers_.pc.full), opcode, ++registers_.pc.full);
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

			case access_program(Inherent):
				perform();
				goto fetch_decode;

			case access_program(Immediate8):
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				perform();
				goto fetch_decode;

			case access_program(Immediate16):
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.high, ++registers_.pc.full);
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				perform();
				goto fetch_decode;
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

	bool mrdy_ = false;
	bool halt_ = false;
	bool dma_ = false;
	bool breq_ = false;

	enum ResumePoint {
		FetchDecode,
		Max,
	};
	using AddressingMode = InstructionSet::M6809::AddressingMode;

	int resume_point_ = ResumePoint::FetchDecode;
	InstructionSet::M6809::Operation operation_;
	Registers registers_;

	enum Exceptions: uint8_t {
		Reset			= 1 << 0,
		PowerOnReset	= 1 << 1,
		NMI				= 1 << 2,
	};
	uint8_t exceptions_ = Exceptions::PowerOnReset;

	// Transient storage.
	Data::Writeable target_;
	RegisterPair16 operand_;
};

}
