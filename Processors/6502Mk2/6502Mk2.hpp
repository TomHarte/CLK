//
//  6502Mk2.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Numeric/RegisterSizes.hpp"
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

struct NoValue {
	operator uint8_t() const { return 0xff; }
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

// MARK: - Registers and flags.

enum class Register {
	LastOperationAddress,
	ProgramCounter,
	StackPointer,
	Flags,
	A,
	X,
	Y,

	//
	// 65816 only.
	//
	EmulationFlag,
	DataBank,
	ProgramBank,
	Direct
};

/*
	Flags as defined on the 6502; can be used to decode the result of @c value_of(Flags) or to form a value for
	the corresponding set.
*/
enum Flag: uint8_t {
	Sign		= 0b1000'0000,
	Overflow	= 0b0100'0000,
	Always		= 0b0010'0000,
	Break		= 0b0001'0000,
	Decimal		= 0b0000'1000,
	Interrupt	= 0b0000'0100,
	Zero		= 0b0000'0010,
	Carry		= 0b0000'0001,

	//
	// 65816 only.
	//
	MemorySize	= Always,
	IndexSize	= Break,
};

struct Flags {
	/// Bit 7 is set if the negative flag is set; otherwise it is clear.
	uint8_t negative_result = 0;

	/// Non-zero if the zero flag is clear, zero if it is set.
	uint8_t zero_result = 0;

	/// Contains Flag::Carry.
	uint8_t carry = 0;

	/// Contains Flag::Decimal.
	uint8_t decimal = 0;

	/// Contains Flag::Overflow.
	uint8_t overflow = 0;

	/// Contains Flag::Interrupt, complemented.
	uint8_t inverse_interrupt = 0;

	/// Sets N and Z flags per the 8-bit value @c value.
	void set_nz(const uint8_t value) {
		zero_result = negative_result = value;
	}

	/// Sets N and Z flags per the 8- or 16-bit value @c value; @c shift should be 0 to indicate an 8-bit value or 8 to indicate a 16-bit value.
	void set_nz(const uint16_t value, const int shift) {
		negative_result = uint8_t(value >> shift);
		zero_result = uint8_t(value | (value >> shift));
	}

	/// Sets the Z flag per the 8- or 16-bit value @c value; @c shift should be 0 to indicate an 8-bit value or 8 to indicate a 16-bit value.
	void set_z(const uint16_t value, const int shift) {
		zero_result = uint8_t(value | (value >> shift));
	}

	/// Sets the N flag per the 8- or 16-bit value @c value; @c shift should be 0 to indicate an 8-bit value or 8 to indicate a 16-bit value.
	void set_n(const uint16_t value, const int shift) {
		negative_result = uint8_t(value >> shift);
	}

	explicit operator uint8_t() const {
		return
			carry | overflow | (inverse_interrupt ^ Flag::Interrupt) | (negative_result & 0x80) |
			(zero_result ? 0 : Flag::Zero) | Flag::Always | Flag::Break | decimal;
	}

	Flags() {
		// Only the interrupt flag is defined upon reset but get_flags isn't going to
		// mask the other flags so we need to do that, at least.
		carry &= Flag::Carry;
		decimal &= Flag::Decimal;
		overflow &= Flag::Overflow;
	}

	Flags(const uint8_t flags) {
		carry				= flags		& Flag::Carry;
		negative_result		= flags		& Flag::Sign;
		zero_result			= (~flags)	& Flag::Zero;
		overflow			= flags		& Flag::Overflow;
		inverse_interrupt	= (~flags)	& Flag::Interrupt;
		decimal				= flags		& Flag::Decimal;
	}
};

// MARK: - Type.

enum Model {
	NES6502,			// The NES's 6502; like a 6502 but lacking decimal mode (though it retains the decimal flag).
	M6502,				// NMOS 6502.
	Synertek65C02,		// A 6502 extended with BRA, P[H/L][X/Y], STZ, TRB, TSB and the (zp) addressing mode, and more.
	Rockwell65C02,		// The Synertek extended with BBR, BBS, RMB and SMB.
	WDC65C02,			// The Rockwell extended with STP and WAI.
	M65816,				// The "16-bit" successor to the 6502.
};
constexpr bool is_8bit(const Model model) { return model <= Model::WDC65C02; }
constexpr bool is_16bit(const Model model) { return model == Model::M65816; }
constexpr bool is_65c02(const Model model) { return model >= Model::Synertek65C02; }

// MARK: - Storage.

/*!
	An opcode that is guaranteed to cause a 6502 to jam.
*/
constexpr uint8_t JamOpcode = 0xf2;

//template <Model model, typename Enable = void> class Storage;
//template <Model model> class Storage<model, std::enable_if_t<true /* is_8bit(model) */>> {

template <Model model, typename Traits> class Storage {
public:
	Storage(Traits::BusHandlerT &bus_handler) noexcept : bus_handler_(bus_handler) {}

	uint16_t value_of(Register) const;
	void set_value_of(Register, uint16_t);

	template <Line line> bool get() const;
	template <Line line> inline void set(bool);

	/// Get whether the 6502 would reset at the next opportunity.
	bool is_resetting() const;

	/*!
		Queries whether the 6502 is now 'jammed'; i.e. has entered an invalid state
		such that it will not of itself perform any more meaningful processing.

		@returns @c true if the 6502 is jammed; @c false otherwise.
	*/
	bool is_jammed() const;

protected:
	Traits::BusHandlerT &bus_handler_;
	uint8_t opcode_, operand_;

	uint8_t a_, x_, y_, s_;
	RegisterPair16 pc_, operation_pc_;
	Flags flags_;

	Cycles cycles_;

	enum ResumePoint {
		FetchDecode,
		Max,
	};
	int resume_point_ = ResumePoint::FetchDecode;

	int return_from_ready_ = 0;
	uint16_t ready_address_ = 0;

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
