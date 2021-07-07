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

/// Tests that the programmable timer flag toggles and produces interrupts
/// at the rate specified, and that the flag toggles when interrupts are signalled.
- (void)performTestExpectedInterrupts:(double)expectedInterruptsPerSecond mode:(int)mode {
	// If a programmable timer mode is requested, synchronise both channels.
	if(mode >= 2) {
		_interruptSource->write(0xa7, 3);
		_interruptSource->run_for(Cycles(2));
	}

	// Set mode (and disable sync, if it was applied).
	_interruptSource->write(0xa7, mode << 5);

	int toggles = 0;
	int interrupts = 0;
	uint8_t dividerState = _interruptSource->get_divider_state() & 1;
	int nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();

	for(int c = 0; c < 250000 * 5; c++) {
		// Advance one cycle. Clock is 500,000 Hz.
		_interruptSource->run_for(Cycles(2));
		--nextSequencePoint;

		// Check for a status bit change.
		const uint8_t newDividerState = _interruptSource->get_divider_state();
		const bool didToggle = (dividerState^newDividerState)&0x1;
		dividerState = newDividerState;
		toggles += didToggle;

		// Check for the relevant interrupt.
		const uint8_t newInterrupts = _interruptSource->get_new_interrupts();
		if(newInterrupts) {
			XCTAssertEqual(nextSequencePoint, 0);
			nextSequencePoint = _interruptSource->get_next_sequence_point().as<int>();

			if(newInterrupts & 0x02) {
				++interrupts;
				XCTAssertTrue(didToggle);
			} else {
				// Failing that, confirm that the other interrupt happend.
				XCTAssertTrue(newInterrupts & 0x08);
			}
		}

		XCTAssertEqual(nextSequencePoint, _interruptSource->get_next_sequence_point().as<int>(), @"At cycle %d", c);
	}

	XCTAssertEqual(toggles, int(expectedInterruptsPerSecond * 5.0));
	XCTAssertEqual(interrupts, int(expectedInterruptsPerSecond * 5.0));
}

- (void)test1kHzTimer {
	[self performTestExpectedInterrupts:1000.0 mode:0];
}

- (void)test50HzTimer {
	[self performTestExpectedInterrupts:50.0 mode:1];
}

- (void)testTone0Timer {
	// Set tone generator 0 as the interrupt source, with a divider of 137;
	// apply sync momentarily.
	_interruptSource->write(0, 137);
	_interruptSource->write(1, 0);

	[self performTestExpectedInterrupts:250000.0/(138.0 * 2.0) mode:2];
}

- (void)testTone1Timer {
	// Set tone generator 1 as the interrupt source, with a divider of 961;
	// apply sync momentarily.
	_interruptSource->write(2, 961 & 0xff);
	_interruptSource->write(3, (961 >> 8) & 0xff);

	[self performTestExpectedInterrupts:250000.0/(962.0 * 2.0) mode:3];
}

@end
