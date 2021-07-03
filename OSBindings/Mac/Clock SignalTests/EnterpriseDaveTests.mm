//
//  EnterpriseDaveTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 02/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Machines/Enterprise/Dave.hpp"
#include <memory>

@interface EnterpriseDaveTests : XCTestCase
@end

@implementation EnterpriseDaveTests {
	std::unique_ptr<Enterprise::Dave::TimedInterruptSource> _interruptSource;
}

- (void)setUp {
	[super setUp];
	_interruptSource = std::make_unique<Enterprise::Dave::TimedInterruptSource>();
}

- (void)testExpectedToggles:(int)expectedToggles expectedInterrupts:(int)expectedInterrupts {
	// Check that the programmable timer flag toggles at a rate
	// of 2kHz, causing 1000 interrupts, and that sequence points
	// are properly predicted.
	int toggles = 0;
	int interrupts = 0;
	uint8_t dividerState = _interruptSource->get_divider_state();
	int nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();
	for(int c = 0; c < 250000; c++) {
		// Advance one cycle. Clock is 250,000 Hz.
		_interruptSource->run_for(Cycles(1));

		const uint8_t newDividerState = _interruptSource->get_divider_state();
		if((dividerState^newDividerState)&0x1) {
			++toggles;
		}
		dividerState = newDividerState;

		--nextSequencePoint;

		// Check for the relevant interrupt.
		const uint8_t newInterrupts = _interruptSource->get_new_interrupts();
		if(newInterrupts & 0x02) {
			++interrupts;
			XCTAssertEqual(nextSequencePoint, 0);
			nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();
		}

		// Failing that, confirm that the other interrupt happend.
		if(!nextSequencePoint) {
			XCTAssertTrue(newInterrupts & 0x08);
			nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();
		}

		XCTAssertEqual(nextSequencePoint, _interruptSource->get_next_sequence_point().as<int>(), @"At cycle %d", c);
	}

	XCTAssertEqual(toggles, expectedToggles);
	XCTAssertEqual(interrupts, expectedInterrupts);
}

- (void)test1kHzTimer {
	// Set 1kHz timer.
	_interruptSource->write(7, 0 << 5);
	[self testExpectedToggles:2000 expectedInterrupts:1000];
}

- (void)test50HzTimer {
	// Set 50Hz timer.
	_interruptSource->write(7, 1 << 5);
	[self testExpectedToggles:100 expectedInterrupts:50];
}

- (void)testTone0Timer {
	// Set tone generator 0 as the interrupt source, with a divider of 137;
	// apply sync momentarily.
	_interruptSource->write(7, 2 << 5);
	_interruptSource->write(0, 137);
	_interruptSource->write(2, 0);

	[self testExpectedToggles:250000/138 expectedInterrupts:250000/(138*2)];
}

@end
