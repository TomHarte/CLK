//
//  ARMDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/ARM/OperationMapper.hpp"

using namespace InstructionSet::ARM;

namespace {

struct Scheduler {
	template <Operation, Flags> void perform(Condition, DataProcessing) {}
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
