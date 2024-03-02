//
//  BarrelShifter.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

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
template <ShiftType type, bool set_carry>
void shift(uint32_t &source, uint32_t amount, typename Carry<set_carry>::type carry) {
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
			if(amount > 32) {
				if constexpr (set_carry) carry = 0;
				source = 0;
			} else if(amount == 32 || !amount) {
				// A logical shift right by '0' is treated as a shift by 32;
				// assemblers are supposed to map LSR #0 to LSL #0.
				if constexpr (set_carry) carry = source & 0x8000'0000;
				source = 0;
			} else {
				if constexpr (set_carry) carry = source & (1 << (amount - 1));
				source >>= amount;
			}
		break;

		case ShiftType::ArithmeticRight: {
			const uint32_t sign = (source & 0x8000'0000) ? 0xffff'ffff : 0x0000'0000;

			if(amount >= 32) {
				if constexpr (set_carry) carry = sign;
				source = sign;
			} else if(amount > 0) {
				if constexpr (set_carry) carry = source & (1 << (amount - 1));
				source = (source >> amount) | (sign << (32 - amount));
			} else {
				// As per logical right, an arithmetic shift of '0' is
				// treated as a shift by 32.
				if constexpr (set_carry) carry = source & 0x8000'0000;
				source = sign;
			}
		} break;

		case ShiftType::RotateRight: {
			if(amount == 32) {
				if constexpr (set_carry) carry = source & 0x8000'0000;
			} else if(amount == 0) {
				// Rotate right by 0 is treated as a rotate right by 1 through carry.
				const uint32_t high = carry << 31;
				if constexpr (set_carry) carry = source & 1;
				source = (source >> 1) | high;
			} else {
				amount &= 31;
				if constexpr (set_carry) carry = source & (1 << (amount - 1));
				source = (source >> amount) | (source << (32 - amount));
			}
		} break;

		// TODO: upon adoption of C++20, use std::rotr.

		default: break;
	}
}

/// Acts as per @c shift above, but applies runtime shift-type selection.
template <bool set_carry>
void shift(ShiftType type, uint32_t &source, uint32_t amount, typename Carry<set_carry>::type carry) {
	switch(type) {
		case ShiftType::LogicalLeft:
			shift<ShiftType::LogicalLeft, set_carry>(source, amount, carry);
		break;
		case ShiftType::LogicalRight:
			shift<ShiftType::LogicalRight, set_carry>(source, amount, carry);
		break;
		case ShiftType::ArithmeticRight:
			shift<ShiftType::ArithmeticRight, set_carry>(source, amount, carry);
		break;
		case ShiftType::RotateRight:
			shift<ShiftType::RotateRight, set_carry>(source, amount, carry);
		break;
	}
}

}
