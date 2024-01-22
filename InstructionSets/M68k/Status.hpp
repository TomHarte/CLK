//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#pragma once

#include "Instruction.hpp"

namespace InstructionSet::M68k {

namespace ConditionCode {

static constexpr uint16_t Carry			= 1 << 0;
static constexpr uint16_t Overflow		= 1 << 1;
static constexpr uint16_t Zero			= 1 << 2;
static constexpr uint16_t Negative		= 1 << 3;
static constexpr uint16_t Extend		= 1 << 4;

static constexpr uint16_t AllConditions	= Carry | Overflow | Zero | Negative | Extend;

static constexpr uint16_t Supervisor	= 1 << 13;
static constexpr uint16_t Trace			= 1 << 15;

static constexpr uint16_t InterruptPriorityMask = 0b111 << 8;

}


struct Status {
	/// Generally holds an unevaluated flag for potential later lazy evaluation; it'll be zero for one outcome,
	/// non-zero for the other, but no guarantees are made about the potential range of non-zero values.
	using FlagT = uint_fast32_t;

	/* b15 */
	FlagT trace_flag = 0;		// The trace flag is set if and only if this value is non-zero.

	/* b13 */
	bool is_supervisor = false;	// true => processor is in supervisor mode; false => it isn't.

	/* b7–b9 */
	int interrupt_level = 0;	// The direct integer value of the current interrupt level.
								// Values of 8 or greater have undefined meaning.

	/* b0–b4 */
	FlagT zero_result = 0;		// The zero flag is set if and only if this value is zero.
	FlagT carry_flag = 0;		// The carry flag is set if and only if this value is non-zero.
	FlagT extend_flag = 0;		// The extend flag is set if and only if this value is non-zero.
	FlagT overflow_flag = 0;	// The overflow flag is set if and only if this value is non-zero.
	FlagT negative_flag = 0;	// The negative flag is set if and only this value is non-zero.

	/// Sets the negative flag per @c value
	template <typename IntT> void set_negative(IntT value) {
		constexpr auto top_bit = IntT(1 << ((sizeof(IntT) * 8) - 1));
		negative_flag = value & top_bit;
	}

	/// Sets both the negative and zero flags according to @c value.
	template <typename IntT> void set_neg_zero(IntT value) {
		zero_result = value;
		set_negative(value);
	}

	/// Gets the current condition codes.
	constexpr uint16_t ccr() const {
		return
			(carry_flag		? ConditionCode::Carry		: 0) |
			(overflow_flag	? ConditionCode::Overflow	: 0) |
			(!zero_result	? ConditionCode::Zero		: 0) |
			(negative_flag	? ConditionCode::Negative	: 0) |
			(extend_flag	? ConditionCode::Extend		: 0);
	}

	/// Sets the current condition codes.
	constexpr void set_ccr(uint16_t ccr) {
		carry_flag		= ccr & ConditionCode::Carry;
		overflow_flag	= ccr & ConditionCode::Overflow;
		zero_result		= ~ccr & ConditionCode::Zero;
		negative_flag	= ccr & ConditionCode::Negative;
		extend_flag		= ccr & ConditionCode::Extend;
	}

	/// Gets the current value of the status register.
	constexpr uint16_t status() const {
		return uint16_t(
			ccr() |
			(interrupt_level << 8) |
			(trace_flag ? ConditionCode::Trace : 0) |
			(is_supervisor ? ConditionCode::Supervisor : 0)
		);
	}

	/// Sets the current value of the status register;
	/// @returns @c true if the processor finishes in supervisor mode; @c false otherwise.
	constexpr bool set_status(uint16_t status) {
		set_ccr(status);

		interrupt_level	= (status >> 8) & 7;
		trace_flag		= status & ConditionCode::Trace;
		is_supervisor	= status & ConditionCode::Supervisor;

		return is_supervisor;
	}

	/// Adjusts the status for exception processing — sets supervisor mode, disables trace,
	/// and if @c new_interrupt_level is greater than or equal to 0 sets that as the new
	/// interrupt level.
	///
	/// @returns The status prior to those changes.
	uint16_t begin_exception(int new_interrupt_level = -1) {
		const uint16_t initial_status = status();

		if(new_interrupt_level >= 0) {
			interrupt_level = new_interrupt_level;
		}
		is_supervisor = true;
		trace_flag = 0;

		return initial_status;
	}

	/// Evaluates @c condition.
	constexpr bool evaluate_condition(Condition condition) const {
		switch(condition) {
			default:
			case Condition::True:			return true;
			case Condition::False:			return false;
			case Condition::High:			return zero_result && !carry_flag;
			case Condition::LowOrSame:		return !zero_result || carry_flag;
			case Condition::CarryClear:		return !carry_flag;
			case Condition::CarrySet:		return carry_flag;
			case Condition::NotEqual:		return zero_result;
			case Condition::Equal:			return !zero_result;
			case Condition::OverflowClear:	return !overflow_flag;
			case Condition::OverflowSet:	return overflow_flag;
			case Condition::Positive:		return !negative_flag;
			case Condition::Negative:		return negative_flag;
			case Condition::GreaterThanOrEqual:
				return (negative_flag && overflow_flag) || (!negative_flag && !overflow_flag);
			case Condition::LessThan:
				return (negative_flag && !overflow_flag) || (!negative_flag && overflow_flag);
			case Condition::GreaterThan:
				return zero_result && ((negative_flag && overflow_flag) || (!negative_flag && !overflow_flag));
			case Condition::LessThanOrEqual:
				return !zero_result || (negative_flag && !overflow_flag) || (!negative_flag && overflow_flag);
		}
	}

	/// @returns @c true if an interrupt at level @c level should be accepted; @c false otherwise.
	constexpr bool would_accept_interrupt(int level) const {
		// TODO: is level seven really non-maskable? If so then what mechanism prevents
		// rapid stack overflow upon a level-seven interrupt?
		return level > interrupt_level;
	}
};

}
