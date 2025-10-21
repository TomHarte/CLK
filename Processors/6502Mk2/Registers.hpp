//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "Numeric/RegisterSizes.hpp"

#pragma once

namespace CPU::MOS6502Mk2 {

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

struct Registers {
	uint8_t a, x, y, s;
	RegisterPair16 pc;
	Flags flags;
	bool is_jammed = false;
};

}
