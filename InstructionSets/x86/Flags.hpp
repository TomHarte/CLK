//
//  Flags.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/10/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_x86_Flags_hpp
#define InstructionSets_x86_Flags_hpp

#include "../../Numeric/Carry.hpp"

namespace InstructionSet::x86 {

namespace FlagValue {

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

enum class Flag {
	Carry,
	AuxiliaryCarry,
	Sign,
	Overflow,
	Trap,
	Interrupt,
	Direction,
	Zero,
	ParityOdd
};

enum class Condition {
	Overflow,
	Below,
	Zero,
	BelowOrEqual,
	Sign,
	ParityOdd,
	Less,
	LessOrEqual
};

class Flags {
	public:
		using FlagT = uint32_t;

		// Flag getters.
		template <Flag flag_v> bool flag() const {
			switch(flag_v) {
				case Flag::Carry:			return carry_;
				case Flag::AuxiliaryCarry:	return auxiliary_carry_;
				case Flag::Sign:			return sign_;
				case Flag::Overflow:		return overflow_;
				case Flag::Trap:			return trap_;
				case Flag::Interrupt:		return interrupt_;
				case Flag::Direction:		return direction_ < 0;
				case Flag::Zero:			return !zero_;
				case Flag::ParityOdd:		return not_parity_bit();
			}
		}

		// Condition evaluation.
		template <Condition test> bool condition() const {
			switch(test) {
				case Condition::Overflow:		return flag<Flag::Overflow>();
				case Condition::Below:			return flag<Flag::Carry>();
				case Condition::Zero:			return flag<Flag::Zero>();
				case Condition::BelowOrEqual:	return flag<Flag::Zero>() || flag<Flag::Carry>();
				case Condition::Sign:			return flag<Flag::Sign>();
				case Condition::ParityOdd:		return flag<Flag::ParityOdd>();
				case Condition::Less:			return flag<Flag::Sign>() != flag<Flag::Overflow>();
				case Condition::LessOrEqual:	return flag<Flag::Zero>() || flag<Flag::Sign>() != flag<Flag::Overflow>();
			}
		}

		// Convenience setters.

		/// Sets all of @c flags as a function of @c value:
		/// • Flag::Zero: sets the zero flag if @c value is zero;
		/// • Flag::Sign: sets the sign flag if the top bit of @c value is one;
		/// • Flag::ParityOdd: sets parity based on the low 8 bits of @c value;
		/// • Flag::Carry: sets carry if @c value is non-zero;
		/// • Flag::AuxiliaryCarry: sets auxiliary carry if @c value is non-zero;
		/// • Flag::Overflow: sets overflow if @c value is non-zero;
		/// • Flag::Interrupt: sets interrupt if @c value is non-zero;
		/// • Flag::Trap: sets interrupt if @c value is non-zero;
		/// • Flag::Direction: sets direction if @c value is non-zero.
		template <typename IntT, Flag... flags> void set_from(IntT value) {
			for(const auto flag: {flags...}) {
				switch(flag) {
					default: break;
					case Flag::Zero:			zero_ = value;								break;
					case Flag::Sign:			sign_ = value & Numeric::top_bit<IntT>();	break;
					case Flag::ParityOdd:		parity_ = value;							break;
					case Flag::Carry:			carry_ = value;								break;
					case Flag::AuxiliaryCarry:	auxiliary_carry_ = value;					break;
					case Flag::Overflow:		overflow_ = value;							break;
					case Flag::Interrupt:		interrupt_ = value;							break;
					case Flag::Trap:			trap_ = value;								break;
					case Flag::Direction:		direction_ = value ? -1 : 1;				break;
				}
			}
		}
		template <Flag... flags> void set_from(FlagT value) {
			set_from<FlagT, flags...>(value);
		}

		template <typename IntT> IntT carry_bit() const { return carry_ ? 1 : 0; }
		bool not_parity_bit() const {
			// x86 parity always considers the lowest 8-bits only.
			auto result = static_cast<uint8_t>(parity_);
			result ^= result >> 4;
			result ^= result >> 2;
			result ^= result >> 1;
			return result & 1;
		}

		template <typename IntT> IntT direction() const { return static_cast<IntT>(direction_); }

		// Complete value get and set.
		void set(uint16_t value) {
			set_from<Flag::Carry>(value & FlagValue::Carry);
			set_from<Flag::AuxiliaryCarry>(value & FlagValue::AuxiliaryCarry);
			set_from<Flag::Overflow>(value & FlagValue::Overflow);
			set_from<Flag::Trap>(value & FlagValue::Trap);
			set_from<Flag::Interrupt>(value & FlagValue::Interrupt);
			set_from<Flag::Direction>(value & FlagValue::Direction);

			set_from<uint8_t, Flag::Sign>(uint8_t(value));

			set_from<Flag::Zero>((~value) & FlagValue::Zero);
			set_from<Flag::ParityOdd>((~value) & FlagValue::Parity);
		}

		uint16_t get() const {
			return
				0xf002 |

				(flag<Flag::Carry>() ? FlagValue::Carry : 0) |
				(flag<Flag::AuxiliaryCarry>() ? FlagValue::AuxiliaryCarry : 0) |
				(flag<Flag::Sign>() ? FlagValue::Sign : 0) |
				(flag<Flag::Overflow>() ? FlagValue::Overflow : 0) |
				(flag<Flag::Trap>() ? FlagValue::Trap : 0) |
				(flag<Flag::Interrupt>() ? FlagValue::Interrupt : 0) |
				(flag<Flag::Direction>() ? FlagValue::Direction : 0) |
				(flag<Flag::Zero>() ? FlagValue::Zero : 0) |

				(flag<Flag::ParityOdd>() ? 0 : FlagValue::Parity);
		}

		std::string to_string() const {
			std::string result;

			if(flag<Flag::Overflow>()) result += "O"; else result += "-";
			if(flag<Flag::Direction>()) result += "D"; else result += "-";
			if(flag<Flag::Interrupt>()) result += "I"; else result += "-";
			if(flag<Flag::Trap>()) result += "T"; else result += "-";
			if(flag<Flag::Sign>()) result += "S"; else result += "-";
			if(flag<Flag::Zero>()) result += "Z"; else result += "-";
			result += "-";
			if(flag<Flag::AuxiliaryCarry>()) result += "A"; else result += "-";
			result += "-";
			if(!flag<Flag::ParityOdd>()) result += "P"; else result += "-";
			result += "-";
			if(flag<Flag::Carry>()) result += "C"; else result += "-";

			return result;
		}

		bool operator ==(const Flags &rhs) const {
			return get() == rhs.get();
		}

	private:
		// Non-zero => set; zero => unset.
		uint32_t carry_;
		uint32_t auxiliary_carry_;
		uint32_t sign_;
		uint32_t overflow_;
		uint32_t trap_;
		uint32_t interrupt_;

		// +1 = direction flag not set;
		// -1 = direction flag set.
		int32_t direction_;

		// Zero => set; non-zero => unset.
		uint32_t zero_;

		// Odd number of bits => set; even => unset.
		uint32_t parity_;
};

}

#endif /* InstructionSets_x86_Flags_hpp */
