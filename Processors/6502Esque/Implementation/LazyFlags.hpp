//
//  LazyFlags.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "../6502Esque.hpp"

namespace CPU::MOS6502Esque {

struct LazyFlags {
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
	void set_nz(uint8_t value) {
		zero_result = negative_result = value;
	}

	/// Sets N and Z flags per the 8- or 16-bit value @c value; @c shift should be 0 to indicate an 8-bit value or 8 to indicate a 16-bit value.
	void set_nz(uint16_t value, int shift) {
		negative_result = uint8_t(value >> shift);
		zero_result = uint8_t(value | (value >> shift));
	}

	/// Sets the Z flag per the 8- or 16-bit value @c value; @c shift should be 0 to indicate an 8-bit value or 8 to indicate a 16-bit value.
	void set_z(uint16_t value, int shift) {
		zero_result = uint8_t(value | (value >> shift));
	}

	/// Sets the N flag per the 8- or 16-bit value @c value; @c shift should be 0 to indicate an 8-bit value or 8 to indicate a 16-bit value.
	void set_n(uint16_t value, int shift) {
		negative_result = uint8_t(value >> shift);
	}

	void set(uint8_t flags) {
		carry				= flags		& Flag::Carry;
		negative_result		= flags		& Flag::Sign;
		zero_result			= (~flags)	& Flag::Zero;
		overflow			= flags		& Flag::Overflow;
		inverse_interrupt	= (~flags)	& Flag::Interrupt;
		decimal				= flags		& Flag::Decimal;
	}

	uint8_t get() const {
		return carry | overflow | (inverse_interrupt ^ Flag::Interrupt) | (negative_result & 0x80) | (zero_result ? 0 : Flag::Zero) | Flag::Always | Flag::Break | decimal;
	}

	LazyFlags() {
		// Only the interrupt flag is defined upon reset but get_flags isn't going to
		// mask the other flags so we need to do that, at least.
		carry &= Flag::Carry;
		decimal &= Flag::Decimal;
		overflow &= Flag::Overflow;
	}
};

}
