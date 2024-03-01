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

//- (void)testXYX {
//	Executor<Memory> scheduler;
//
//	for(int c = 0; c < 65536; c++) {
//		InstructionSet::ARM::dispatch(c << 16, scheduler);
//	}
//	InstructionSet::ARM::dispatch(0xEAE06900, scheduler);
//}

- (void)testBarrelShifterLogicalRight {
	uint32_t value;
	uint32_t carry;

	// Test successive shifts by 4; one generating carry and one not.
	value = 0x12345678;
	shift<ShiftType::LogicalRight, true>(value, 4, carry);
	XCTAssertEqual(value, 0x1234567);
	XCTAssertNotEqual(carry, 0);

	shift<ShiftType::LogicalRight, true>(value, 4, carry);
	XCTAssertEqual(value, 0x123456);
	XCTAssertEqual(carry, 0);

	// Test shift by 1.
	value = 0x8003001;
	shift<ShiftType::LogicalRight, true>(value, 1, carry);
	XCTAssertEqual(value, 0x4001800);
	XCTAssertNotEqual(carry, 0);

	// Test a shift by greater than 32.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 33, carry);
	XCTAssertEqual(value, 0);
	XCTAssertEqual(carry, 0);

	// Test shifts by 32: result is always 0, carry is whatever was in bit 31.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 32, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);

	value = 0x7fff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 32, carry);
	XCTAssertEqual(value, 0);
	XCTAssertEqual(carry, 0);

	// Test that a logical right shift by 0 is the same as a shift by 32.
	value = 0xffff'ffff;
	shift<ShiftType::LogicalRight, true>(value, 0, carry);
	XCTAssertEqual(value, 0);
	XCTAssertNotEqual(carry, 0);
}

@end
