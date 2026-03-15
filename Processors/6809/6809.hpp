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
		#define addressing_program(name)	int(ResumePoint::Max) + int(name)
		#define access_program(name)		int(ResumePoint::Max) + int(AddressingMode::Max) + int(name)
		#define restore_point()				(__COUNTER__ - FirstCounter + int(ResumePoint::Max) + int(AddressingMode::Max) + int(AccessType::Max))

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

		// TODO: non-MRDY version.
		#define write(bus_state, addr, value, ...) {																\
			time_ -= Cycles(1);																						\
			time_ -= bus_handler_.template perform<BusPhase::FullCycle, ReadWrite::Write, bus_state>(addr, value);	\
																													\
			__VA_ARGS__;																							\
		}

		time_ += duration;

		using Literal = Address::Literal;

		InstructionSet::M6809::OperationReturner op_returner;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page0> op_mapper0;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page1> op_mapper1;
		InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page2> op_mapper2;

		const auto perform = [&]() {
			CPU::M6809::perform(operation_.operation, registers_, operand_);
		};

		uint8_t opcode = 0;
		while(true) switch(resume_point_) {
			default: {
				__builtin_unreachable();
			}

			// MARK: - Exceptions.

			reset:
				registers_.dp = 0;
				exceptions_ &= ~(Exceptions::NMI | Exceptions::PowerOnReset);
				registers_.cc.set<ConditionCode::IRQMask>(true);
				registers_.cc.set<ConditionCode::FIRQMask>(true);

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

				printf("--- Fetch\n");
				read(BusState::Normal, Literal(registers_.pc.full), opcode, ++registers_.pc.full);
				{
					operation_ = Reflection::dispatch(op_mapper0, opcode, op_returner);
					resume_point_ = addressing_program(operation_.mode);
					break;
				}

			fetch_decode_page1:
				read(BusState::Normal, Literal(registers_.pc.full), opcode, ++registers_.pc.full);
				{
					operation_ = Reflection::dispatch(op_mapper1, opcode, op_returner);
					resume_point_ = addressing_program(operation_.mode);
					break;
				}

			fetch_decode_page2:
				read(BusState::Normal, Literal(registers_.pc.full), opcode, ++registers_.pc.full);
				{
					operation_ = Reflection::dispatch(op_mapper2, opcode, op_returner);
					resume_point_ = addressing_program(operation_.mode);
					break;
				}

			// MARK: - Variant addressing mode.

			case addressing_program(AddressingMode::Variant):
				if(operation_.operation == Operation::Page2) {
					goto fetch_decode_page2;
				} else {
					goto fetch_decode_page1;
				}

			// MARK: - Inherent addressing mode.

			case addressing_program(AddressingMode::Inherent):
				perform();
				goto fetch_decode;

			// MARK: - Immediate and relative addressing modes.

			case addressing_program(AddressingMode::Relative8):
				if(operation_.operation == Operation::BSR) {
					goto bsr;
				}
				[[fallthrough]];
			case addressing_program(AddressingMode::Immediate8):
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				perform();
				goto fetch_decode;

			case addressing_program(AddressingMode::Relative16):
				if(operation_.operation == Operation::BSR) {
					goto lbsr;
				}
				[[fallthrough]];
			case addressing_program(AddressingMode::Immediate16):
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.high, ++registers_.pc.full);
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				perform();
				goto fetch_decode;

			// MARK: - Direct addressing mode.

			case addressing_program(AddressingMode::Direct):
				read(BusState::Normal, Literal(registers_.pc.full), address_.halves.low, ++registers_.pc.full);
				address_.halves.high = registers_.reg<R8::DP>();
				resume_point_ = access_program(operation_.type);
				break;

			// MARK: - Extended addressing mode.

			case addressing_program(AddressingMode::Extended):
				read(BusState::Normal, Literal(registers_.pc.full), address_.halves.high, ++registers_.pc.full);
				read(BusState::Normal, Literal(registers_.pc.full), address_.halves.low, ++registers_.pc.full);
				resume_point_ = access_program(operation_.type);
				break;


			// MARK: - Indexed addressing mode.

			case addressing_program(AddressingMode::Indexed):

				//
				// Determine target address.
				//

				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				indexer_ = IndexedAddressDecoder(operand_.halves.low);

				if(!indexer_.required_continuation()) {
					goto continue_indexed;
				} else if(indexer_.required_continuation() == 1) {
					operand_.halves.high = 0;
					goto indexed_getlow;
				}

				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.high, ++registers_.pc.full);

			indexed_getlow:
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				indexer_.set_continuation(operand_.full);

			continue_indexed:
				address_.full = indexer_.address(registers_);
				if(!indexer_.indirect()) {
					goto complete_address;
				}

				read(BusState::Normal, Literal(address_.full), operand_.halves.high, ++address_.full);
				read(BusState::Normal, Literal(address_.full), operand_.halves.low, ++address_.full);
				address_ = operand_;

			complete_address:
				resume_point_ = access_program(operation_.type);
				break;

			// MARK: - 'Specialised' addresing mode (i.e. irregulars0.

			case addressing_program(AddressingMode::Specialised):
				switch(operation_.operation) {
					case Operation::PULU:
					case Operation::PULS:
						goto pull;

					case Operation::RTS:
						goto rts;

					case Operation::SWI:	case Operation::SWI2:	case Operation::SWI3:
						goto swi;

					default: __builtin_unreachable();
				}

			// MARK: - PULU/PULS.

			pull:
				stack_ = operation_.operation == Operation::PULU ? &registers_.reg<R16::U>(): &registers_.reg<R16::S>();
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);

				if(!(operand_.halves.low & 0b0000'0001)) {
					goto no_pull_cc;
				}
				read(BusState::Normal, Literal(*stack_), registers_.reg<R8::CC>(), ++*stack_);
			no_pull_cc:

				if(!(operand_.halves.low & 0b0000'0010)) {
					goto no_pull_a;
				}
				read(BusState::Normal, Literal(*stack_), registers_.reg<R8::A>(), ++*stack_);
			no_pull_a:

				if(!(operand_.halves.low & 0b0000'0100)) {
					goto no_pull_b;
				}
				read(BusState::Normal, Literal(*stack_), registers_.reg<R8::B>(), ++*stack_);
			no_pull_b:

				if(!(operand_.halves.low & 0b0000'1000)) {
					goto no_pull_dp;
				}
				read(BusState::Normal, Literal(*stack_), registers_.reg<R8::DP>(), ++*stack_);
			no_pull_dp:

				if(!(operand_.halves.low & 0b0001'0000)) {
					goto no_pull_x;
				}
				read(BusState::Normal, Literal(*stack_), address_.halves.high, ++*stack_);
				read(BusState::Normal, Literal(*stack_), address_.halves.low, ++*stack_);
				registers_.reg<R16::X>() = address_.full;
			no_pull_x:

				if(!(operand_.halves.low & 0b0010'0000)) {
					goto no_pull_y;
				}
				read(BusState::Normal, Literal(*stack_), address_.halves.high, ++*stack_);
				read(BusState::Normal, Literal(*stack_), address_.halves.low, ++*stack_);
				registers_.reg<R16::Y>() = address_.full;
			no_pull_y:

				if(!(operand_.halves.low & 0b0100'0000)) {
					goto no_pull_s;
				}
				read(BusState::Normal, Literal(*stack_), address_.halves.high, ++*stack_);
				read(BusState::Normal, Literal(*stack_), address_.halves.low, ++*stack_);
				(operation_.operation == Operation::PULU ? registers_.reg<R16::S>() : registers_.reg<R16::U>())
					= address_.full;
			no_pull_s:

				if(!(operand_.halves.low & 0b1000'0000)) {
					goto fetch_decode;
				}
				read(BusState::Normal, Literal(*stack_), address_.halves.high, ++*stack_);
				read(BusState::Normal, Literal(*stack_), address_.halves.low, ++*stack_);
				registers_.reg<R16::PC>() = address_.full;
				goto fetch_decode;

			// MARK: - Stack-related control flow.

			rts:
				read(BusState::Normal, Literal(registers_.reg<R16::S>()), address_.halves.high, ++registers_.reg<R16::S>());
				read(BusState::Normal, Literal(registers_.reg<R16::S>()), address_.halves.low, ++registers_.reg<R16::S>());
				registers_.reg<R16::PC>() = address_.full;
				goto fetch_decode;

			bsr:
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				address_.full = uint16_t(registers_.pc.full + int8_t(operand_.halves.low));
				goto jsr;

			lbsr:
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.high, ++registers_.pc.full);
				read(BusState::Normal, Literal(registers_.pc.full), operand_.halves.low, ++registers_.pc.full);
				address_.full = registers_.pc.full + operand_.full;
				goto jsr;

			swi:
				registers_.cc.set<ConditionCode::Entire>(true);

				operand_ = registers_.reg<R16::PC>();
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.low);
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.high);

				operand_ = registers_.reg<R16::U>();
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.low);
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.high);

				operand_ = registers_.reg<R16::Y>();
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.low);
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.high);

				operand_ = registers_.reg<R16::X>();
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.low);
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), operand_.halves.high);

				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), registers_.reg<R8::DP>());
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), registers_.reg<R8::B>());
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), registers_.reg<R8::A>());
				-- registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), registers_.reg<R8::CC>());

				if(operation_.operation == Operation::SWI) {
					registers_.cc.set<ConditionCode::IRQMask>(true);
					registers_.cc.set<ConditionCode::FIRQMask>(true);
					address_.full = 0xfffa;
				} else {
					address_.full = operation_.operation == Operation::SWI2 ? 0xfff4 : 0xfff2;
				}

				read(BusState::Normal, Literal(address_.full), registers_.pc.halves.high, ++address_.full);
				read(BusState::Normal, Literal(address_.full), registers_.pc.halves.low, ++address_.full);
				goto fetch_decode;


			//
			// Access atoms.
			//
			case access_program(AccessType::LEA):
				operand_.full = address_.full;
				perform();
				goto fetch_decode;

			case access_program(AccessType::JSR):
			jsr:
				--registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), registers_.pc.halves.low);
				--registers_.reg<R16::S>();
				write(BusState::Normal, Literal(registers_.reg<R16::S>()), registers_.pc.halves.high);
				registers_.reg<R16::PC>() = address_.full;
				goto fetch_decode;

			case access_program(AccessType::Read8):
				read(BusState::Normal, Literal(address_.full), operand_.halves.low);
				perform();
				goto fetch_decode;

			case access_program(AccessType::Read16):
				read(BusState::Normal, Literal(address_.full), operand_.halves.high, ++address_.full);
				read(BusState::Normal, Literal(address_.full), operand_.halves.low);
				perform();
				goto fetch_decode;

			case access_program(AccessType::Write8):
				perform();
				write(BusState::Normal, Literal(address_.full), operand_.halves.low);
				goto fetch_decode;

			case access_program(AccessType::Write16):
				perform();
				write(BusState::Normal, Literal(address_.full), operand_.halves.high, ++address_.full);
				write(BusState::Normal, Literal(address_.full), operand_.halves.low);
				goto fetch_decode;

			case access_program(AccessType::Modify8):
				read(BusState::Normal, Literal(address_.full), operand_.halves.low);
				perform();
				write(BusState::Normal, Literal(address_.full), operand_.halves.low);
				goto fetch_decode;
		}

		#undef access_label
		#undef attach
		#undef join
		#undef access
		#undef restore_point
	}

	Registers registers() {
		return registers_;
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
	using Operation = InstructionSet::M6809::Operation;
	using AddressingMode = InstructionSet::M6809::AddressingMode;
	using AccessType = InstructionSet::M6809::AccessType;

	int resume_point_ = ResumePoint::FetchDecode;
	InstructionSet::M6809::OperationReturner::MetaOperation operation_;
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
	RegisterPair16 address_;
	IndexedAddressDecoder indexer_;
	uint16_t *stack_ = nullptr;
};

}
