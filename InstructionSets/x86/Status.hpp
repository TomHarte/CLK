//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_x86_Status_hpp
#define InstructionSets_x86_Status_hpp


namespace InstructionSet::x86 {

namespace ConditionCode {

//
// Standard flags.
//

static constexpr uint32_t Carry				= 1 << 0;
static constexpr uint32_t Parity			= 1 << 2;
static constexpr uint32_t AuxiliaryCarry	= 1 << 4;
static constexpr uint32_t Zero				= 1 << 6;
static constexpr uint32_t Sign				= 1 << 7;
static constexpr uint32_t Trap				= 1 << 8;
static constexpr uint32_t Interrupt			= 1 << 9;
static constexpr uint32_t Direction			= 1 << 10;
static constexpr uint32_t Overflow			= 1 << 11;

//
// 80286+ additions.
//

static constexpr uint32_t IOPrivilege		= (1 << 12) | (1 << 13);
static constexpr uint32_t NestedTask		= 1 << 14;

//
// 16-bit protected mode flags.
//

static constexpr uint32_t ProtectionEnable				= 1 << 16;
static constexpr uint32_t MonitorProcessorExtension		= 1 << 17;
static constexpr uint32_t ProcessorExtensionExtension	= 1 << 18;
static constexpr uint32_t TaskSwitch					= 1 << 19;

//
// 32-bit protected mode flags.
//

static constexpr uint32_t Resume			= 1 << 16;
static constexpr uint32_t VirtualMode		= 1 << 17;

}

struct Status {
	// Non-zero => set; zero => unset.
	uint32_t carry;
	uint32_t auxiliary_carry;
	uint32_t sign;
	uint32_t overflow;
	uint32_t trap;
	uint32_t interrupt;
	uint32_t direction;

	// Zero => set; non-zero => unset.
	uint32_t zero;

	// Odd number of bits => set; even => unset.
	uint32_t parity;

	// Flag getters.
	template <typename IntT> IntT carry_bit() const { return carry ? 1 : 0; }
	bool parity_bit() const {
		uint32_t result = parity;
		result ^= result >> 16;
		result ^= result >> 8;
		result ^= result >> 4;
		result ^= result >> 2;
		result ^= result >> 1;
		return 1 ^ (result & 1);
	}

	// Complete value get and set.
	void set(uint16_t value) {
		carry = value & ConditionCode::Carry;
		auxiliary_carry = value & ConditionCode::AuxiliaryCarry;
		sign = value & ConditionCode::Sign;
		overflow = value & ConditionCode::Overflow;
		trap = value & ConditionCode::Trap;
		interrupt = value & ConditionCode::Interrupt;
		direction = value & ConditionCode::Direction;

		zero = (~value) & ConditionCode::Zero;

		parity = (~value) & ConditionCode::Parity;
	}

	uint16_t get() const {
		return
			0xf002 |

			(carry ? ConditionCode::Carry : 0) |
			(auxiliary_carry ? ConditionCode::AuxiliaryCarry : 0) |
			(sign ? ConditionCode::Sign : 0) |
			(overflow ? ConditionCode::Overflow : 0) |
			(trap ? ConditionCode::Trap : 0) |
			(interrupt ? ConditionCode::Interrupt : 0) |
			(direction ? ConditionCode::Direction : 0) |

			(zero ? 0 : ConditionCode::Zero) |

			(parity_bit() ? ConditionCode::Parity : 0);
	}

	bool operator ==(const Status &rhs) const {
		return get() == rhs.get();
	}
};

}

#endif /* InstructionSets_x86_Status_hpp */
