//
//  ARMDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/ARM/OperationMapper.hpp"
#include "../../../InstructionSets/ARM/Status.hpp"
#include "../../../Numeric/Carry.hpp"

using namespace InstructionSet::ARM;

namespace {

template <ShiftType type, bool set_carry>
void shift(uint32_t &source, uint32_t amount, uint32_t *carry = nullptr) {
	switch(type) {
		case ShiftType::LogicalLeft:
			if(amount > 32) {
				if constexpr (set_carry) *carry = 0;
				source = 0;
			} else if(amount == 32) {
				if constexpr (set_carry) *carry = source & 1;
				source = 0;
			} else {
				if constexpr (set_carry) *carry = source & (0x8000'0000 >> amount);
				source <<= amount;
			}
		break;

		default: break;
	}
}

template <bool set_carry>
void shift(ShiftType type, uint32_t &source, uint32_t amount, uint32_t *carry) {
	switch(type) {
		case ShiftType::LogicalLeft:
			shift<ShiftType::LogicalLeft, set_carry>(source, amount, carry);
		break;
		case ShiftType::LogicalRight:
			shift<ShiftType::LogicalRight, set_carry>(source, amount, carry);
		break;
		case ShiftType::ArithmeticRight:
			shift<ShiftType::ArithmeticRight, set_carry>(source, amount, carry);
		break;
		case ShiftType::RotateRight:
			shift<ShiftType::RotateRight, set_carry>(source, amount, carry);
		break;
	}
}


struct Scheduler {
	bool should_schedule(Condition condition) {
		return status.test(condition);
	}

	template <Flags f> void perform(DataProcessing fields) {
		// TODO: how does register 15 fit into all of below? As an operand or as a target?

		constexpr DataProcessingFlags flags(f);
		auto &destination = registers_[fields.destination()];
		const uint32_t operand1 = registers_[fields.operand1()];
		uint32_t operand2;
		uint32_t rotate_carry = 0;

		// Populate carry from the shift only if it'll be used.
		constexpr bool shift_sets_carry = is_logical(flags.operation()) && flags.set_condition_codes();

		// Get operand 2.
		if constexpr (flags.operand2_is_immediate()) {
			operand2 = fields.immediate();
			if(fields.rotate()) {
				shift<ShiftType::RotateRight, shift_sets_carry>(operand2, fields.rotate(), &rotate_carry);
			}
		} else {
			uint32_t shift_amount;
			if(fields.shift_count_is_register()) {
				shift_amount = registers_[fields.shift_register()];
			} else {
				shift_amount = fields.shift_amount();
			}

			operand2 = registers_[fields.operand2()];
			shift<shift_sets_carry>(fields.shift_type(), operand2, shift_amount, &rotate_carry);
		}

		// Perform the data processing operation.
		uint32_t conditions = 0;
		switch(flags.operation()) {
			// Logical operations.
			case DataProcessingOperation::AND:	conditions = destination = operand1 & operand2;		break;
			case DataProcessingOperation::EOR:	conditions = destination = operand1 ^ operand2;		break;
			case DataProcessingOperation::ORR:	conditions = destination = operand1 | operand2;		break;
			case DataProcessingOperation::BIC:	conditions = destination = operand1 & ~operand2;	break;

			case DataProcessingOperation::MOV:	conditions = destination = operand2;	break;
			case DataProcessingOperation::MVN:	conditions = destination = ~operand2;	break;

			case DataProcessingOperation::TST:	conditions = operand1 & operand2;	break;
			case DataProcessingOperation::TEQ:	conditions = operand1 ^ operand2;	break;

			case DataProcessingOperation::ADD:
			case DataProcessingOperation::ADC:
			case DataProcessingOperation::CMN:
				conditions = operand1 + operand2;

				if constexpr (flags.operation() == DataProcessingOperation::ADC) {
					conditions += status.c();
				}

				if constexpr (flags.set_condition_codes()) {
					status.set_c(Numeric::carried_out<true, 31>(operand1, operand2, conditions));
					status.set_v(Numeric::overflow<true>(operand1, operand2, conditions));
				}

				if constexpr (!is_comparison(flags.operation())) {
					destination = conditions;
				}
			break;

			case DataProcessingOperation::SUB:
			case DataProcessingOperation::SBC:
			case DataProcessingOperation::CMP:
				conditions = operand1 - operand2;

				if constexpr (flags.operation() == DataProcessingOperation::SBC) {
					conditions -= status.c();
				}

				if constexpr (flags.set_condition_codes()) {
					status.set_c(Numeric::carried_out<false, 31>(operand1, operand2, conditions));
					status.set_v(Numeric::overflow<false>(operand1, operand2, conditions));
				}

				if constexpr (!is_comparison(flags.operation())) {
					destination = conditions;
				}
			break;

			case DataProcessingOperation::RSB:
			case DataProcessingOperation::RSC:
				conditions = operand2 - operand1;

				if constexpr (flags.operation() == DataProcessingOperation::RSC) {
					conditions -= status.c();
				}

				if constexpr (flags.set_condition_codes()) {
					status.set_c(Numeric::carried_out<false, 31>(operand2, operand1, conditions));
					status.set_v(Numeric::overflow<false>(operand2, operand1, conditions));
				}

				destination = conditions;
			break;
		}

		// Set N and Z in a unified way.
		if constexpr (flags.set_condition_codes()) {
			status.set_nz(conditions);
		}

		// Set C from the barrel shifter if applicable.
		if constexpr (shift_sets_carry) {
			status.set_c(rotate_carry);
		}

		// TODO: If register 15 was in use as a destination, write back and clean up.
	}

	template <Operation, Flags> void perform(Condition, Multiply) {}
	template <Operation, Flags> void perform(Condition, SingleDataTransfer) {}
	template <Operation, Flags> void perform(Condition, BlockDataTransfer) {}

	template <Operation op> void perform(Condition condition, Branch branch) {
		printf("Branch %sif %d; add %08x\n", op == Operation::BL ? "with link " : "", int(condition), branch.offset());
	}
	template <Operation, Flags> void perform(Condition, CoprocessorRegisterTransfer) {}
	template <Flags> void perform(Condition, CoprocessorDataOperation) {}
	template<Operation, Flags> void perform(Condition, CoprocessorDataTransfer) {}

	void software_interrupt(Condition) {}
	void unknown(uint32_t) {}

private:
	Status status;

	uint32_t registers_[16];	// TODO: register swaps with mode.
};

}

@interface ARMDecoderTests : XCTestCase
@end

@implementation ARMDecoderTests

- (void)testXYX {
	Scheduler scheduler;

	for(int c = 0; c < 65536; c++) {
		InstructionSet::ARM::dispatch(c << 16, scheduler);
	}
	InstructionSet::ARM::dispatch(0xEAE06900, scheduler);
}

@end
