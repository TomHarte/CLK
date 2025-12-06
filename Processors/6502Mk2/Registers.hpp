//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#include "Numeric/RegisterSizes.hpp"

#include <cassert>
#include <compare>

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
	Negative	= 0b1000'0000,
	Overflow	= 0b0100'0000,
	Always		= 0b0010'0000,
	Break		= 0b0001'0000,
	Decimal		= 0b0000'1000,
	Interrupt	= 0b0000'0100,
	Zero		= 0b0000'0010,
	Carry		= 0b0000'0001,

	//
	// Psuedo-flags, for convenience setting and getting.
	//
	NegativeZero		= 0b00,
	InverseInterrupt	= 0b11,

	//
	// 65816 only.
	//
	MemorySize	= Always,
	IndexSize	= Break,
};

constexpr bool is_stored(const Flag flag) {
	switch(flag) {
		case Flag::Negative:
		case Flag::Overflow:
		case Flag::Decimal:
		case Flag::Interrupt:
		case Flag::InverseInterrupt:
		case Flag::Zero:
		case Flag::Carry:
			return true;

		default:
			return false;
	}
}

constexpr bool is_settable(const Flag flag) {
	switch(flag) {
		case Flag::NegativeZero:	return true;
		default:					return is_stored(flag);
	}
}

struct Flags {
	template <Flag flag>
	requires(is_stored(flag))
	bool get() const {
		switch(flag) {
			default:						__builtin_unreachable();
			case Flag::Negative:			return negative_result & Flag::Negative;
			case Flag::Overflow:			return overflow;
			case Flag::Decimal:				return decimal;
			case Flag::Interrupt:			return !(inverse_interrupt & Flag::Interrupt);
			case Flag::InverseInterrupt:	return inverse_interrupt & Flag::Interrupt;
			case Flag::Zero:				return !zero_result;
			case Flag::Carry:				return carry & Flag::Carry;
		}
		return false;
	}

	/// Sets a flag based on an 8-bit ALU result.
	template <Flag flag>
	requires(is_settable(flag))
	void set_per(const uint8_t result) {
		switch(flag) {
			default:	__builtin_unreachable();

			case Flag::Negative:			negative_result = result;											break;
			case Flag::Overflow:			overflow = result & Flag::Overflow;									break;
			case Flag::Decimal:				decimal = result & Flag::Decimal;									break;
			case Flag::Interrupt:			inverse_interrupt = uint8_t(~Flag::Interrupt) | uint8_t(~result);	break;
			case Flag::InverseInterrupt:	inverse_interrupt = uint8_t(~Flag::Interrupt) | result;				break;
			case Flag::Zero:				zero_result = result;												break;
			case Flag::Carry:
				assert(!(result & ~Flag::Carry));
				carry = result;
			break;
			case Flag::NegativeZero:		zero_result = negative_result = result;								break;
		}
	}

	template <Flag flag>
	requires(is_settable(flag))
	void set(const bool result) {
		set_per<flag>(result ? flag : 0x00);
	}

	uint8_t carry_value() const {
		return carry;
	}

	uint8_t interrupt_mask() const {
		return inverse_interrupt;
	}

	void set_overflow(const uint8_t result, const uint8_t lhs, const uint8_t rhs) {
		// TODO: can this be done lazily?
		overflow = (( (result^lhs) & (result^rhs) ) & 0x80) >> 1;
	}

	explicit operator uint8_t() const {
		return
			carry | overflow | ((~inverse_interrupt) & Flag::Interrupt) | (negative_result & 0x80) |
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
		set_per<Flag::Carry>(flags & Flag::Carry);
		set_per<Flag::Negative>(flags);
		set_per<Flag::Zero>((~flags) & Flag::Zero);
		set_per<Flag::Overflow>(flags);
		set_per<Flag::Interrupt>(flags);
		set_per<Flag::Decimal>(flags);

		assert((flags | Flag::Always | Flag::Break) == static_cast<uint8_t>(*this));
	}

	auto operator <=> (const Flags &rhs) const {
		return static_cast<uint8_t>(*this) <=> static_cast<uint8_t>(rhs);
	}
	auto operator ==(const Flags &rhs) const {
		return static_cast<uint8_t>(*this) == static_cast<uint8_t>(rhs);
	}

private:
	uint8_t overflow = 0;			/// Contains Flag::Overflow.
	uint8_t carry = 0;				/// Contains Flag::Carry.
	uint8_t negative_result = 0;	/// Bit 7 = the negative flag.
	uint8_t zero_result = 0;		/// Non-zero if the zero flag is clear, zero if it is set.
	uint8_t decimal = 0;			/// Contains Flag::Decimal.
	uint8_t inverse_interrupt = 0;	/// Contains Flag::Interrupt, complemented.
};

struct Registers {
	uint8_t a, x, y, s;
	RegisterPair16 pc;
	Flags flags;

	auto operator <=> (const Registers &) const = default;
};

}
