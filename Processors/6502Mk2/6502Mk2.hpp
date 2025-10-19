//
//  6502Mk2.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace CPU::MOS6502Mk2 {

// MARK: - Address resolvers.

namespace Address {

struct Literal {
	constexpr LiteralAddress(const uint16_t address) noexcept : address_(address);
	operator uint16_t() const {
		return address_;
	}

private:
	uint16_t address_;
};

struct ZeroPage {
	constexpr ZeroPage(const uint8_t address) noexcept : address_(address);
	operator uint16_t() const {
		return address_;
	}

private:
	uint8_t address_;
};

struct Stack {
	constexpr ZeroPage(const uint8_t address) noexcept : address_(address);
	operator uint16_t() const {
		return 0x0100 | address_;
	}

private:
	uint8_t address_;
};

struct Vector {
	constexpr ZeroPage(const uint8_t address) noexcept : address_(address);
	operator uint16_t() const {
		return 0xff00 | address_;
	}

private:
	uint8_t address_;
};

}  // namespace Address

namespace Data {

struct NoValue {
	operator uint8_t() const { return 0xff; }
};

/*!
	Bus handlers perform bus transactions, connecting the 6502-esque chip to the rest of the syste.
	@c BusOperation provides the type of operation.
*/
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

template <BusOperation, typename Enable = void> struct Value;
template <BusOperation operation> struct Value<operation, std::enable_if_t<is_read(operation)> {
	using type = uint8_t &;
};
template <BusOperation operation> struct Value<operation, std::enable_if_t<is_write(operation)> {
	using type = const uint8_t;
};
template <BusOperation operation> struct Value<operation, std::enable_if_t<is_dataless(operation)> {
	using type = const NoValue;
};

} // namespace Data

/*
	The list of registers that can be accessed via @c value_of(Register) and @c set_value_of(Register, value).
*/
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

enum Personality {
	PNES6502,			// The NES's 6502; like a 6502 but lacking decimal mode (though it retains the decimal flag).
	P6502,				// NMOS 6502.
	PSynertek65C02,		// A 6502 extended with BRA, P[H/L][X/Y], STZ, TRB, TSB and the (zp) addressing mode, and more.
	PRockwell65C02,		// The Synertek extended with BBR, BBS, RMB and SMB.
	PWDC65C02,			// The Rockwell extended with STP and WAI.
	P65816,				// The "16-bit" successor to the 6502.
};

}
