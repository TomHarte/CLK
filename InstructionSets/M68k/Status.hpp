//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M68k_Status_h
#define InstructionSets_M68k_Status_h

#include "Instruction.hpp"

namespace InstructionSet {
namespace M68k {

struct Status {
	enum ConditionCode: uint16_t {
		Carry		= (1 << 0),
		Overflow	= (1 << 1),
		Zero		= (1 << 2),
		Negative	= (1 << 3),
		Extend		= (1 << 4),

		Supervisor	= (1 << 13),
		Trace		= (1 << 15),

		InterruptPriorityMask = (0b111 << 8),
	};

	/* b15 */
	uint_fast32_t trace_flag = 0;		// The trace flag is set if this value is non-zero.

	/* b13 */
	int is_supervisor = 0;				// 1 => processor is in supervisor mode; 0 => it isn't.

	/* b7–b9 */
	int interrupt_level = 0;			// The direct integer value of the current interrupt level.

	/* b0–b4 */
	uint_fast32_t zero_result = 0;		// The zero flag is set if this value is zero.
	uint_fast32_t carry_flag = 0;		// The carry flag is set if this value is non-zero.
	uint_fast32_t extend_flag = 0;		// The extend flag is set if this value is non-zero.
	uint_fast32_t overflow_flag = 0;	// The overflow flag is set if this value is non-zero.
	uint_fast32_t negative_flag = 0;	// The negative flag is set if this value is non-zero.

	/// Gets the current condition codes.
	constexpr uint16_t ccr() const {
		return
			(carry_flag 	? ConditionCode::Carry 		: 0) |
			(overflow_flag	? ConditionCode::Overflow	: 0) |
			(zero_result	? 0							: ConditionCode::Zero) |
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
			(is_supervisor << 13)
		);
	}

	/// Sets the current value of the status register;
	/// @returns @c true if the processor finishes in supervisor mode; @c false otherwise.
	constexpr bool set_status(uint16_t status) {
		set_ccr(status);

		interrupt_level	= (status >> 8) & 7;
		trace_flag		= status & ConditionCode::Trace;
		is_supervisor	= (status >> 13) & 1;

		return is_supervisor;
	}

	/// Evaluates @c condition.
	bool evaluate_condition(Condition condition) {
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
};

}
}

#endif /* InstructionSets_M68k_Status_h */
