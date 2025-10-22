//
//  6502Mk2.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Decoder.hpp"
#include "Model.hpp"
#include "Perform.hpp"
#include "Registers.hpp"

#include "ClockReceiver/ClockReceiver.hpp"

#include <type_traits>

namespace CPU::MOS6502Mk2 {

// MARK: - Control bus.

enum class BusOperation {
	/// 6502: a read was signalled.
	/// 65816: a read was signalled with VDA.
	Read,
	/// 6502: a read was signalled with SYNC.
	/// 65816: a read was signalled with VDA and VPA.
	ReadOpcode,
	/// 6502: never signalled.
	/// 65816: a read was signalled with VPA.
	ReadProgram,
	/// 6502: never signalled.
	/// 65816: a read was signalled with VPB and VDA.
	ReadVector,
	/// 6502: never signalled.
	/// 65816: a read was signalled, but neither VDA nor VPA were active.
	InternalOperationRead,

	/// All processors: indicates that the processor is paused due to the RDY input.
	/// 65C02 and 65816: indicates a WAI is ongoing.
	Ready,

	/// 65C02 and 65816: indicates a STP condition.
	None,

	/// 6502: a write was signalled.
	/// 65816: a write was signalled with VDA.
	Write,
	/// 6502: never signalled.
	/// 65816: a write was signalled, but neither VDA nor VPA were active.
	InternalOperationWrite,
};

constexpr bool is_read(const BusOperation op) { return op <= BusOperation::InternalOperationRead; }
constexpr bool is_write(const BusOperation op) { return op >= BusOperation::Write; }
constexpr bool is_access(const BusOperation op) { return op <= BusOperation::ReadVector || op == BusOperation::Write; }
constexpr bool is_dataless(const BusOperation op) { return !is_read(op) && !is_write(op); }

enum class Line {
	Reset,
	IRQ,
	PowerOn,
	Overflow,
	NMI,
};

// MARK: - Address bus.

namespace Address {

struct Literal {
	constexpr Literal(const uint16_t address) noexcept : address_(address) {}
	operator uint16_t() const {
		return address_;
	}

private:
	uint16_t address_;
};

struct ZeroPage {
	constexpr ZeroPage(const uint8_t address) noexcept : address_(address) {}
	operator uint16_t() const {
		return address_;
	}

private:
	uint8_t address_;
};

struct Stack {
	constexpr Stack(const uint8_t address) noexcept : address_(address) {}
	operator uint16_t() const {
		return 0x0100 | address_;
	}

private:
	uint8_t address_;
};

struct Vector {
	constexpr Vector(const uint8_t address) noexcept : address_(address) {}
	operator uint16_t() const {
		return 0xff00 | address_;
	}

private:
	uint8_t address_;
};

}  // namespace Address

// MARK: - Data bus.

namespace Data {

/// A value that can be read from or written to, without effect.
struct NoValue {
	operator uint8_t() const { return 0xff; }
	NoValue() = default;
	constexpr NoValue(uint8_t) noexcept {}
};

template <BusOperation, typename Enable = void> struct Value;
template <BusOperation operation> struct Value<operation, std::enable_if_t<is_read(operation)>> {
	using type = uint8_t &;
};
template <BusOperation operation> struct Value<operation, std::enable_if_t<is_write(operation)>> {
	using type = const uint8_t;
};
template <BusOperation operation> struct Value<operation, std::enable_if_t<is_dataless(operation)>> {
	using type = const NoValue;
};

} // namespace Data

template <BusOperation operation> using data_t = typename Data::Value<operation>::type;

// MARK: - Storage.

/*!
	An opcode that is guaranteed to cause a 6502 to jam.
*/
constexpr uint8_t JamOpcode = 0xf2;

template <Model model, typename Traits, typename Enable = void> class Storage;
template <Model model, typename Traits> class Storage<model, Traits, std::enable_if_t<is_8bit(model)>> {
public:
	Storage(Traits::BusHandlerT &bus_handler) noexcept : bus_handler_(bus_handler) {}

	const Registers &registers() const { return registers_; }
	void set_registers(const Registers &registers) {
		registers_ = registers;
	}

	template <Line line> bool get() const;
	template <Line line> inline void set(const bool value) {
		const auto set_interrupt_request = [&](const Inputs::InterruptRequest request) {
			inputs_.interrupt_requests =
				(inputs_.interrupt_requests & ~request) |
				(value ? request : 0);
		};

		switch(line) {
			case Line::Reset:		set_interrupt_request(Inputs::InterruptRequest::Reset);		break;
			case Line::PowerOn:		set_interrupt_request(Inputs::InterruptRequest::PowerOn);	break;
			case Line::IRQ:			set_interrupt_request(Inputs::InterruptRequest::IRQ);		break;

			// These are both edge triggered, I think?
//			case Line::Overflow:	set_interrupt_request(Inputs::InterruptRequest::Reset);	break;
//			case Line::NMI:			set_interrupt_request(Inputs::InterruptRequest::NMI);		break;

			default:
				__builtin_unreachable();
		}
	}

	/// Get whether the 6502 would reset at the next opportunity.
	bool is_resetting() const;

	/*!
		Queries whether the 6502 is now 'jammed'; i.e. has entered an invalid state
		such that it will not of itself perform any more meaningful processing.

		@returns @c true if the 6502 is jammed; @c false otherwise.
	*/
	bool is_jammed() const {
		return resume_point_ == ResumePoint::Jam;
	}

protected:
	Traits::BusHandlerT &bus_handler_;
	uint8_t opcode_, operand_;
	Instruction decoded_;

	Registers registers_;
	uint16_t operation_pc_;
	RegisterPair16 address_;

	Cycles cycles_;

	enum ResumePoint {
		FetchDecode,
		Jam,
		Max,
	};
	int resume_point_ = ResumePoint::FetchDecode;

	struct Inputs {
		bool ready = false;

		enum InterruptRequest: uint8_t {
			Reset		= 0x80,
			IRQ			= Flag::Interrupt,
			NMI			= 0x20,

			PowerOn		= 0x10,
		};
		uint8_t interrupt_requests = InterruptRequest::PowerOn;
	} inputs_;
};

// MARK: - Base.

enum class PausePrecision {
	BetweenInstructions,
	AnyCycle,
};

// TODO: concept to explain and verify Traits.
template <Model model, typename Traits>
struct Processor: public Storage<model, Traits> {
	inline void run_for(Cycles);

	/*!
		**FOR TEST CASES ONLY:** forces the processor into a state where
		the next thing it intends to do is fetch a new opcode.
	*/
	inline void restart_operation_fetch();

private:
	using Storage = Storage<model, Traits>;
};

}

// MARK: - Implementations.
#include "Implementation/6502.hpp"
