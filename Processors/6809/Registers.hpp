//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include "Numeric/RegisterSizes.hpp"

namespace CPU::M6809 {

enum ConditionCode: uint8_t {
	Entire		= 0b1000'0000,
	FIRQMask	= 0b0100'0000,
	HalfCarry	= 0b0010'0000,
	IRQMask		= 0b0001'0000,
	Negative	= 0b0000'1000,
	Zero		= 0b0000'0100,
	Overflow	= 0b0000'0010,
	Carry		= 0b0000'0001,
};

struct ConditionCodeRegister {
	ConditionCodeRegister() = default;
	ConditionCodeRegister(const uint8_t value) {
		*this = value;
	}

	template <ConditionCode code>
	bool condition() const {
		switch(code) {
			case ConditionCode::Entire:		return entire_;
			case ConditionCode::FIRQMask:	return firq_;
			case ConditionCode::HalfCarry:	return half_carry_;
			case ConditionCode::IRQMask:	return irq_;
			case ConditionCode::Negative:	return negative_ & 0x80;
			case ConditionCode::Zero:		return !zero_;
			case ConditionCode::Overflow:	return overflow_ & 0x80;
			case ConditionCode::Carry:		return carry_;
		}
	}

	template <ConditionCode code>
	void set(const bool value) {
		switch(code) {
			case ConditionCode::Entire:		entire_ = value;					break;
			case ConditionCode::FIRQMask:	firq_ = value;						break;
			case ConditionCode::HalfCarry:	half_carry_ = value ? 0x10 : 0x00;	break;
			case ConditionCode::IRQMask:	irq_ = value;						break;
			case ConditionCode::Negative:	negative_ = value ? 0xff : 0x00;	break;
			case ConditionCode::Zero:		zero_ = value ? 0x00 : 0xff;		break;
			case ConditionCode::Overflow:	overflow_ = value ? 0xff : 0x00;	break;
			case ConditionCode::Carry:		carry_ = value ? 0x01 : 0x00;		break;
		}
	}

	uint8_t operator =(const uint8_t rhs) {
		entire_ = rhs & ConditionCode::Entire;
		firq_ = rhs & ConditionCode::FIRQMask;
		half_carry_ = uint8_t((rhs & ConditionCode::HalfCarry) >> 1);
		irq_ = rhs & ConditionCode::IRQMask;
		negative_ = uint8_t((rhs & ConditionCode::Negative) << 4);
		zero_ = !(rhs & ConditionCode::Zero);
		overflow_ = uint8_t((rhs & ConditionCode::Overflow) << 6);
		carry_ = rhs & ConditionCode::Carry;
		return rhs;
	}

	operator uint8_t() const {
		return uint8_t(
			(entire_ ? ConditionCode::Entire : 0) |
			(firq_ ? ConditionCode::FIRQMask : 0) |
			(half_carry_ << 1) |
			(irq_ ? ConditionCode::IRQMask : 0) |
			((negative_ & 0x80) ? ConditionCode::Negative : 0) |
			(zero_ ? 0 : ConditionCode::Zero) |
			((overflow_ & 0x80) ? ConditionCode::Overflow : 0) |
			carry_
		);
	}

	template <std::unsigned_integral ValueT>
	requires (sizeof(ValueT) <= 2)
	void set_nz(const ValueT value) {
		if constexpr (std::is_same_v<ValueT, uint8_t>) {
			negative_ = zero_ = value;
		} else {
			negative_ = uint8_t(value >> 8);
			zero_ = uint8_t(value | (value >> 8));
		}
	}

	template <std::unsigned_integral ValueT>
	requires (sizeof(ValueT) <= 2)
	void set_overflow(const ValueT result, const ValueT lhs, const ValueT rhs) {
		const ValueT bits = (result^lhs) & (result^rhs);
		if constexpr (std::is_same_v<ValueT, uint8_t>) {
			overflow_ = bits;
		} else {
			overflow_ = uint8_t(bits >> 8);
		}
	}

	uint8_t carry() const { return carry_; }
	uint8_t half_carry() const { return half_carry_; }

private:
	uint8_t negative_ = 0;
	uint8_t zero_ = 0;
	uint8_t overflow_ = 0;
	uint8_t carry_ = 0;
	bool irq_ = true;
	uint8_t half_carry_ = 0;
	bool firq_ = true;
	bool entire_ = false;
};

enum R8 {
	A,
	B,
	CC,
	DP,
};
enum R16 {
	D,
	X,
	Y,
	S,
	U,
	PC,
};

struct Registers {
	uint16_t x;
	uint16_t y;
	uint16_t u;
	uint16_t s;
	RegisterPair16 pc;
	RegisterPair16 d;
	uint8_t dp;
	ConditionCodeRegister cc;

	template <R8 r>
	auto &reg() {
		if constexpr (r == R8::CC) {
			return cc;
		} else {
			switch(r) {
				case R8::A:		return d.halves.high;
				case R8::B:		return d.halves.low;
				case R8::DP:	return dp;
				default:	__builtin_unreachable();
			}
		}
	}

	template <R16 r>
	uint16_t &reg() {
		switch(r) {
			case R16::D:	return d.full;
			case R16::X:	return x;
			case R16::Y:	return y;
			case R16::S:	return s;
			case R16::U:	return u;
			case R16::PC:	return pc.full;
			default:	__builtin_unreachable();
		}
	}
};

}
