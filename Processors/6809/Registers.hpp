//
//  Registers.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

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
};

struct Registers {
	uint16_t x;
	uint16_t y;
	uint16_t u;
	uint16_t s;
	uint16_t pc;
	uint8_t a;
	uint8_t b;
	uint8_t dp;

	ConditionCodeRegister cc;
};

}
