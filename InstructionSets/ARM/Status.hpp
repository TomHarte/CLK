//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "OperationMapper.hpp"

#include <cstdint>

namespace InstructionSet::ARM {

namespace ConditionCode {

static constexpr uint32_t Negative		= 1 << 31;
static constexpr uint32_t Zero			= 1 << 30;
static constexpr uint32_t Carry			= 1 << 29;
static constexpr uint32_t Overflow		= 1 << 28;
static constexpr uint32_t IRQDisable	= 1 << 27;
static constexpr uint32_t FIQDisable	= 1 << 26;
static constexpr uint32_t Mode			= (1 << 1) | (1 << 0);

static constexpr uint32_t Address		= FIQDisable - Mode - 1;

}

enum class Mode {
	User = 0b00,
	FIQ = 0b01,
	IRQ = 0b10,
	Supervisor = 0b11,
};

struct Status {
	public:
		/// Sets the N and Z flags according to the value of @c result.
		void set_nz(uint32_t value) {
			zero_result_ = negative_flag_ = value;
		}

		/// Sets C if @c value is non-zero; resets it otherwise.
		void set_c(uint32_t value) {
			carry_flag_ = value;
		}

		/// @returns @c 1 if carry is set; @c 0 otherwise.
		uint32_t c() const {
			return carry_flag_ ? 1 : 0;
		}

		/// Sets V if the highest bit of @c value is set; resets it otherwise.
		void set_v(uint32_t value) {
			overflow_flag_ = value;
		}

		/// @returns The program counter address only, optionally with a post-increment.
		template <bool increment>
		uint32_t pc() {
			const uint32_t result = pc_;
			if constexpr (increment) {
				pc_ = (pc_ + 4) & ConditionCode::Address;
			}
			return result;
		}

		void begin_irq() {	interrupt_flags_ |= ConditionCode::IRQDisable;	}
		void begin_fiq() {	interrupt_flags_ |= ConditionCode::FIQDisable;	}

		/// @returns The full PC + status bits.
		uint32_t get() const {
			return
				uint32_t(mode_) |
				pc_ |
				(negative_flag_ & ConditionCode::Negative) |
				(zero_result_ ? 0 : ConditionCode::Zero) |
				(carry_flag_ ? ConditionCode::Carry : 0) |
				((overflow_flag_ >> 3) & ConditionCode::Overflow) |
				interrupt_flags_;
		}

		bool test(Condition condition) {
			const auto ne = [&]() -> bool {
				return zero_result_;
			};
			const auto cs = [&]() -> bool {
				return carry_flag_;
			};
			const auto mi = [&]() -> bool {
				return negative_flag_ & ConditionCode::Negative;
			};
			const auto vs = [&]() -> bool {
				return overflow_flag_ & ConditionCode::Negative;
			};
			const auto hi = [&]() -> bool {
				return carry_flag_ && zero_result_;
			};
			const auto lt = [&]() -> bool {
				return (negative_flag_ ^ overflow_flag_) & ConditionCode::Negative;
			};
			const auto le = [&]() -> bool {
				return !zero_result_ || lt();
			};

			switch(condition) {
				case Condition::EQ:	return !ne();
				case Condition::NE:	return ne();
				case Condition::CS:	return cs();
				case Condition::CC:	return !cs();
				case Condition::MI:	return mi();
				case Condition::PL:	return !mi();
				case Condition::VS:	return vs();
				case Condition::VC:	return !vs();

				case Condition::HI:	return hi();
				case Condition::LS:	return !hi();
				case Condition::GE:	return !lt();
				case Condition::LT:	return lt();
				case Condition::GT:	return !le();
				case Condition::LE:	return le();

				case Condition::AL:	return true;
				case Condition::NV:	return false;
			}
		}

	private:
		uint32_t pc_ = 0;
		Mode mode_ = Mode::Supervisor;

		uint32_t zero_result_ = 0;
		uint32_t negative_flag_ = 0;
		uint32_t interrupt_flags_ = 0;
		uint32_t carry_flag_ = 0;
		uint32_t overflow_flag_ = 0;
};

}
