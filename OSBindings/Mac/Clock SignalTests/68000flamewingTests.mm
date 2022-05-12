//
//  68000ComparativeTests.cpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 14/12/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/M68k/Perform.hpp"

using namespace InstructionSet::M68k;

@interface M68000flamewingTests : XCTestCase
@end

@implementation M68000flamewingTests {
	int _testsPerformed;
}

- (Status)statusWithflamewingFlags:(int)flags {
	Status status;
	status.carry_flag = status.extend_flag = flags & 2;
	status.zero_result = ~flags & 1;
	status.negative_flag = 0;
	status.overflow_flag = 0;
	return status;
}

- (void)validate:(const uint8_t *)test source:(int)source dest:(int)dest flags:(int)flags result:(uint32_t)result status:(Status)status operation:(NSString *)operation {
	const uint8_t result_flags = test[0];
	const uint8_t result_value = test[1];

	++_testsPerformed;
	NSString *const testName =
		[NSString stringWithFormat:@"%@ %02x, %02x [%c%c]", operation, source, dest, (flags & 2) ? 'X' : '-', (flags & 1) ? 'Z' : '-'];
	XCTAssertEqual(result, uint32_t(result_value), @"Wrong value received for %@", testName);
	XCTAssertEqual(status.ccr(), uint16_t(result_flags), @"Wrong status received for %@", testName);
}

- (void)testAll {
	// Get the full list of available test files.
	NSBundle *const bundle = [NSBundle bundleForClass:[self class]];
	NSURL *const testURL = [bundle URLForResource:@"bcd-table" withExtension:@"bin" subdirectory:@"flamewing 68000 BCD tests"];
	NSData *const testData = [NSData dataWithContentsOfURL:testURL];
	const uint8_t *bytes = reinterpret_cast<const uint8_t *>(testData.bytes);

	NullFlowController flow_controller;

	// Test ABCD.
	for(int source = 0; source < 256; source++) {
		for(int dest = 0; dest < 256; dest++) {
			for(int flags = 0; flags < 4; flags++) {
				Status status = [self statusWithflamewingFlags:flags];

				CPU::SlicedInt32 s, d;
				s.l = source;
				d.l = dest;

				perform<Model::M68000, NullFlowController, Operation::ABCD>(
					Preinstruction(), s, d, status, flow_controller);

				[self validate:bytes source:source dest:dest flags:flags result:d.l status:status operation:@"ABCD"];
				bytes += 2;
			}
		}
	}

	// Test SBCD.
	for(int source = 0; source < 256; source++) {
		for(int dest = 0; dest < 256; dest++) {
			for(int flags = 0; flags < 4; flags++) {
				Status status = [self statusWithflamewingFlags:flags];

				CPU::SlicedInt32 s, d;
				s.l = source;
				d.l = dest;

				perform<Model::M68000, NullFlowController, Operation::SBCD>(
					Preinstruction(), s, d, status, flow_controller);

				[self validate:bytes source:source dest:dest flags:flags result:d.l status:status operation:@"SBCD"];
				bytes += 2;
			}
		}
	}

	// Test NBCD.
	for(int source = 0; source < 256; source++) {
		for(int flags = 0; flags < 4; flags++) {
			Status status = [self statusWithflamewingFlags:flags];

			CPU::SlicedInt32 s, d;
			s.l = source;

			perform<Model::M68000, NullFlowController, Operation::NBCD>(
				Preinstruction(), s, d, status, flow_controller);

			[self validate:bytes source:source dest:0 flags:flags result:s.l status:status operation:@"NBCD"];
			bytes += 2;
		}
	}

	NSLog(@"%d tests performed", _testsPerformed);
}

@end
