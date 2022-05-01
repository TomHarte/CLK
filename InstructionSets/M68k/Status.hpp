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
	/* b15 */
	uint_fast32_t trace_flag_ = 0;		// The trace flag is set if this value is non-zero.

	/* b13 */
	int is_supervisor_ = 0;				// 1 => processor is in supervisor mode; 0 => it isn't.

	/* b7–b9 */
	int interrupt_level_ = 0;			// The direct integer value of the current interrupt level.

	/* b0–b4 */
	uint_fast32_t zero_result_ = 0;		// The zero flag is set if this value is zero.
	uint_fast32_t carry_flag_ = 0;		// The carry flag is set if this value is non-zero.
	uint_fast32_t extend_flag_ = 0;		// The extend flag is set if this value is non-zero.
	uint_fast32_t overflow_flag_ = 0;	// The overflow flag is set if this value is non-zero.
	uint_fast32_t negative_flag_ = 0;	// The negative flag is set if this value is non-zero.

	/// Gets the current condition codes.
	constexpr uint16_t ccr() const {
		return
			(carry_flag_ 	? 0x0001 : 0x0000) |
			(overflow_flag_	? 0x0002 : 0x0000) |
			(zero_result_	? 0x0000 : 0x0004) |
			(negative_flag_	? 0x0008 : 0x0000) |
			(extend_flag_	? 0x0010 : 0x0000);
	}

	/// Gets the current value of the status register.
	constexpr uint16_t status() const {
		return uint16_t(
			ccr() |
			(interrupt_level_ << 8) |
			(trace_flag_ ? 0x8000 : 0x0000) |
			(is_supervisor_ << 13)
		);
	}

	/// Sets the current condition codes.
	constexpr void set_ccr(uint16_t ccr) {
		carry_flag_			= (ccr) & 0x0001;
		overflow_flag_		= (ccr) & 0x0002;
		zero_result_		= ((ccr) & 0x0004) ^ 0x0004;
		negative_flag_		= (ccr) & 0x0008;
		extend_flag_		= (ccr) & 0x0010;
	}

	/// Sets the current value of the status register;
	/// @returns @c true if the processor finishes in supervisor mode; @c false otherwise.
	constexpr bool set_status(uint16_t status) {
		set_ccr(status);

		interrupt_level_ 	= (status >> 8) & 7;
		trace_flag_			= status & 0x8000;
		is_supervisor_		= (status >> 13) & 1;

		return is_supervisor_;
	}

	/// Evaluates @c condition.
	bool evaluate_condition(Condition condition) {
		switch(condition) {
			default:
			case Condition::True:			return true;
			case Condition::False:			return false;
			case Condition::High:			return zero_result_ && !carry_flag_;
			case Condition::LowOrSame:		return !zero_result_ || carry_flag_;
			case Condition::CarryClear:		return !carry_flag_;
			case Condition::CarrySet:		return carry_flag_;
			case Condition::NotEqual:		return zero_result_;
			case Condition::Equal:			return !zero_result_;
			case Condition::OverflowClear:	return !overflow_flag_;
			case Condition::OverflowSet:	return overflow_flag_;
			case Condition::Positive:		return !negative_flag_;
			case Condition::Negative:		return negative_flag_;
			case Condition::GreaterThanOrEqual:
				return (negative_flag_ && overflow_flag_) || (!negative_flag_ && !overflow_flag_);
			case Condition::LessThan:
				return (negative_flag_ && !overflow_flag_) || (!negative_flag_ && overflow_flag_);
			case Condition::GreaterThan:
				return zero_result_ && ((negative_flag_ && overflow_flag_) || (!negative_flag_ && !overflow_flag_));
			case Condition::LessThanOrEqual:
				return !zero_result_ || (negative_flag_ && !overflow_flag_) || (!negative_flag_ && overflow_flag_);
		}
	}
};

}
}

#endif /* InstructionSets_M68k_Status_h */
