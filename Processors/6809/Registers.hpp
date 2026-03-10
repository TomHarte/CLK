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
	template <ConditionCode code>
	bool condition() const {
		switch(code) {
			case ConditionCode::Negative:	return sign_ & 0x80;
			case ConditionCode::Zero:		return !zero_;
			case ConditionCode::Overflow:	return overflow_;
		}
	}

	void set_nz(const uint8_t value) {
		sign_ = zero_ = value;
	}

	template <ConditionCode code>
	void set(const bool value) {
		switch(code) {
			case ConditionCode::Negative:	sign_ = value ? 0xff : 0x00;	break;
			case ConditionCode::Zero:		zero_ = value ? 0x00 : 0xff;	break;
			case ConditionCode::Overflow:	overflow_ = value;				break;
		}
	}

private:
	uint8_t sign_;
	uint8_t zero_;
	bool overflow_;
};

enum R8 {
	A,
	B,
};
enum R16 {
	D,
	X,
	Y,
	S,
	U,
};

struct Registers {
	uint16_t x;
	uint16_t y;
	uint16_t u;
	uint16_t s;
	RegisterPair16 pc;
	RegisterPair16 d;
	uint8_t dp;

	template <R8 r>
	uint8_t &reg() {
		switch(r) {
			case R8::A:	return d.halves.high;
			case R8::B:	return d.halves.low;
			default:	__builtin_unreachable();
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
			default:	__builtin_unreachable();
		}
	}

	ConditionCodeRegister cc;
};

}
