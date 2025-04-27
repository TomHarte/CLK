//
//  BarrelShifter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include <bit>

namespace InstructionSet::ARM {

enum class ShiftType {
	LogicalLeft = 0b00,
	LogicalRight = 0b01,
	ArithmeticRight = 0b10,
	RotateRight = 0b11,
};

template <bool writeable> struct Carry;
template <> struct Carry<true> {
	using type = uint32_t &;
};
template <> struct Carry<false> {
	using type = const uint32_t;
};

/// Apply a rotation of @c type to @c source of @c amount; @c carry should be either @c 1 or @c 0
/// at call to represent the current value of the carry flag. If @c set_carry is @c true then @c carry will
/// receive the new value of the carry flag following the rotation — @c 0 for no carry, @c non-0 for carry.
///
/// Shift amounts of 0 are given the meaning attributed to them for immediate shift counts.
template <ShiftType type, bool set_carry, bool is_immediate_shift>
void shift(uint32_t &source, uint32_t amount, const typename Carry<set_carry>::type carry) {
	switch(type) {
		case ShiftType::LogicalLeft:
			if(amount > 32) {
				if constexpr (set_carry) carry = 0;
				source = 0;
			} else if(amount == 32) {
				if constexpr (set_carry) carry = source & 1;
				source = 0;
			} else if(amount > 0) {
				if constexpr (set_carry) carry = source & (0x8000'0000 >> (amount - 1));
				source <<= amount;
			}
		break;

		case ShiftType::LogicalRight:
			if(!amount && is_immediate_shift) {
				// An immediate logical shift right by '0' is treated as a shift by 32;
				// assemblers are supposed to map LSR #0 to LSL #0.
				amount = 32;
			}

			if(amount > 32) {
				if constexpr (set_carry) carry = 0;
				source = 0;
			} else if(amount == 32) {
				if constexpr (set_carry) carry = source & 0x8000'0000;
				source = 0;
			} else if(amount > 0) {
				if constexpr (set_carry) carry = source & (1 << (amount - 1));
				source >>= amount;
			}
		break;

		case ShiftType::ArithmeticRight: {
			if(!amount && is_immediate_shift) {
				// An immediate arithmetic shift of '0' is treated as a shift by 32.
				amount = 32;
			}

			const uint32_t sign = (source & 0x8000'0000) ? 0xffff'ffff : 0x0000'0000;

			if(amount >= 32) {
				if constexpr (set_carry) carry = source & 0x8000'0000;
				source = sign;
			} else if(amount > 0) {
				if constexpr (set_carry) carry = source & (1 << (amount - 1));
				source = (source >> amount) | (sign << (32 - amount));
			}
		} break;

		case ShiftType::RotateRight: {
			if(!amount) {
				if(is_immediate_shift) {
					// Immediate rotate right by 0 is treated as a rotate right by 1 through carry.
					const uint32_t high = carry << 31;
					if constexpr (set_carry) carry = source & 1;
					source = (source >> 1) | high;
				}
				break;
			}

			// "ROR by 32 has result equal to Rm, carry out equal to bit 31 ...
			// [for] ROR by n where n is greater than 32 ... repeatedly subtract 32 from n
			// until the amount is in the range 1 to 32"
			amount &= 31;
			if(amount) {
				if constexpr (set_carry) carry = source & (1 << (amount - 1));
				source = std::rotr(source, int(amount));
			} else {
				if constexpr (set_carry) carry = source & 0x8000'0000;
			}
		} break;

		default: break;
	}
}

/// Acts as per @c shift above, but applies runtime shift-type selection.
template <bool set_carry, bool is_immediate_shift>
void shift(const ShiftType type, uint32_t &source, const uint32_t amount, const typename Carry<set_carry>::type carry) {
	switch(type) {
		case ShiftType::LogicalLeft:
			shift<ShiftType::LogicalLeft, set_carry, is_immediate_shift>(source, amount, carry);
		break;
		case ShiftType::LogicalRight:
			shift<ShiftType::LogicalRight, set_carry, is_immediate_shift>(source, amount, carry);
		break;
		case ShiftType::ArithmeticRight:
			shift<ShiftType::ArithmeticRight, set_carry, is_immediate_shift>(source, amount, carry);
		break;
		case ShiftType::RotateRight:
			shift<ShiftType::RotateRight, set_carry, is_immediate_shift>(source, amount, carry);
		break;
	}
}

}
