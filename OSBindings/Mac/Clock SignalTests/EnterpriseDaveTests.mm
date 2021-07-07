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

- (void)performTestExpectedInterrupts:(int)expectedInterrupts {
	// Check that the programmable timer flag toggles at a rate
	// of 2kHz, causing 1000 interrupts, and that sequence points
	// are properly predicted.
	int toggles = 0;
	int interrupts = 0;
	uint8_t dividerState = _interruptSource->get_divider_state() & 1;
	int nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();

	for(int c = 0; c < 250000; c++) {
		// Advance one cycle. Clock is 500,000 Hz.
		_interruptSource->run_for(Cycles(2));

		const uint8_t newDividerState = _interruptSource->get_divider_state();
		const bool didToggle = (dividerState^newDividerState)&0x1;
		if(didToggle) {
			++toggles;
		}
		dividerState = newDividerState;

		--nextSequencePoint;

		// Check for the relevant interrupt.
		const uint8_t newInterrupts = _interruptSource->get_new_interrupts();
		if(newInterrupts & 0x02) {
			++interrupts;
			XCTAssertEqual(nextSequencePoint, 0);
			XCTAssertTrue(didToggle);
			nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();
		}

		// Failing that, confirm that the other interrupt happend.
		if(!nextSequencePoint) {
			XCTAssertTrue(newInterrupts & 0x08);
			nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();
		}

		XCTAssertEqual(nextSequencePoint, _interruptSource->get_next_sequence_point().as<int>(), @"At cycle %d", c);
	}

	XCTAssertEqual(toggles, expectedInterrupts);
	XCTAssertEqual(interrupts, expectedInterrupts);
}

- (void)test1kHzTimer {
	// Set 1kHz timer.
	_interruptSource->write(7, 0 << 5);
	[self performTestExpectedInterrupts:1000];
}

- (void)test50HzTimer {
	// Set 50Hz timer.
	_interruptSource->write(7, 1 << 5);
	[self performTestExpectedInterrupts:50];
}

- (void)testTone0Timer {
	// Set tone generator 0 as the interrupt source, with a divider of 137;
	// apply sync momentarily.
	_interruptSource->write(7, 2 << 5);
	_interruptSource->write(0, 137);
	_interruptSource->write(1, 0);

	[self performTestExpectedInterrupts:250000/(138 * 2)];
}

- (void)testTone1Timer {
	// Set tone generator 1 as the interrupt source, with a divider of 961;
	// apply sync momentarily.
	_interruptSource->write(7, 3 << 5);
	_interruptSource->write(2, 961 & 0xff);
	_interruptSource->write(3, (961 >> 8) & 0xff);

	[self performTestExpectedInterrupts:250000/(961 * 2)];
}

@end
