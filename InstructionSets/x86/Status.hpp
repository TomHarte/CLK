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

	// Zero => set; non-zero => unset.
	uint32_t zero;

	// Odd number of bits => set; even => unset.
	uint32_t parity;

	// Convenience getters.
	template <typename IntT> IntT carry_bit() { return carry ? 1 : 0; }

	void set(uint16_t value) {
		carry = value & ConditionCode::Carry;

		// TODO: the rest.
	}
};

}

#endif /* InstructionSets_x86_Status_hpp */
