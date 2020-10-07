//
//  LazyFlags.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef LazyFlags_h
#define LazyFlags_h

#include "../6502Esque.hpp"

namespace CPU {
namespace MOS6502Esque {

struct LazyFlags {
	/// Bit 7 is set if the negative flag is set; otherwise it is clear.
	uint8_t negative_result;

	/// Non-zero if the zero flag is clear, zero if it is set.
	uint8_t zero_result;

	/// Contains Flag::Carry.
	uint8_t carry;

	/// Contains Flag::Decimal.
	uint8_t decimal;

	/// Contains Flag::Overflow.
	uint8_t overflow;

	/// Contains Flag::Interrupt, complemented.
	uint8_t inverse_interrupt = 0;

	void set_nz(uint8_t value) {
		zero_result = negative_result = value;
	}

	void set_nz(uint16_t value, int shift) {
		negative_result = uint8_t(value >> shift);
		zero_result = uint8_t(value | (value >> shift));
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
		return carry | overflow | (inverse_interrupt ^ Flag::Interrupt) | (negative_result & 0x80) | (zero_result ? 0 : Flag::Zero) | Flag::Always | decimal;
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
}

#endif /* LazyFlags_h */
