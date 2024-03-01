//
//  ARMDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/ARM/Executor.hpp"

using namespace InstructionSet::ARM;

namespace {

struct Memory {
	template <typename IntT>
	bool write(uint32_t address, IntT source, Mode mode, bool trans) {
		(void)address;
		(void)source;
		(void)mode;
		(void)trans;
		return true;
	}

	template <typename IntT>
	bool read(uint32_t address, IntT &source, Mode mode, bool trans) {
		(void)address;
		(void)source;
		(void)mode;
		(void)trans;
		return true;
	}
};

}

@interface ARMDecoderTests : XCTestCase
@end

@implementation ARMDecoderTests

- (void)testXYX {
	Executor<Memory> scheduler;

	for(int c = 0; c < 65536; c++) {
		InstructionSet::ARM::dispatch(c << 16, scheduler);
	}
	InstructionSet::ARM::dispatch(0xEAE06900, scheduler);
}

@end
