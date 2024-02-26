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

using namespace InstructionSet::ARM;

namespace {

template <ShiftType type>
void shift(uint32_t &source, uint32_t &carry, uint32_t amount) {
	switch(type) {
		case ShiftType::LogicalLeft:
			if(amount > 32) {
				source = carry = 0;
			} else if(amount == 32) {
				carry = source & 1;
				source = 0;
			} else {
				carry = source & (0x8000'0000 >> amount);
				source <<= amount;
			}
		break;

		default: break;
	}
}

void shift(ShiftType type, uint32_t &source, uint32_t &carry, uint32_t amount) {
	switch(type) {
		case ShiftType::LogicalLeft:		shift<ShiftType::LogicalLeft>(source, carry, amount);		break;
		case ShiftType::LogicalRight:		shift<ShiftType::LogicalRight>(source, carry, amount);		break;
		case ShiftType::ArithmeticRight:	shift<ShiftType::ArithmeticRight>(source, carry, amount);	break;
		case ShiftType::RotateRight:		shift<ShiftType::RotateRight>(source, carry, amount);		break;
	}
}

struct Scheduler {
	template <Flags f> void perform(Condition condition, DataProcessing fields) {
		if(!status.test(condition)) {
			return;
		}

		constexpr DataProcessingFlags flags(f);
		auto &destination = registers_[fields.destination()];
		const auto &operand1 = registers_[fields.operand1()];
		uint32_t operand2;
		uint32_t rotate_carry = 0;

		if constexpr (flags.operand2_is_immediate()) {
			if(!fields.rotate()) {
				operand2 = fields.immediate();
			} else {
				operand2 = fields.immediate() >> fields.rotate();
				operand2 |= fields.immediate() << (32 - fields.rotate());
			}
		} else {
			uint32_t shift_amount;
			if(fields.shift_count_is_register()) {
				shift_amount = registers_[fields.shift_register()];
			} else {
				shift_amount = fields.shift_amount();
			}

			operand2 = registers_[fields.operand2()];
			shift(fields.shift_type(), operand2, rotate_carry, shift_amount);
		}

		switch(flags.operation()) {
			case DataProcessingOperation::AND:
				destination = operand1 & operand2;
			break;

			default: break;	// ETC.
		}
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

	InstructionSet::ARM::dispatch(0xEAE06900, scheduler);
//	const auto intr = Instruction<Model::ARM2>(1);
//	NSLog(@"%d", intr.operation());
}

@end
