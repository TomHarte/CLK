//
//  MasterSystemVDPTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 09/10/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "9918.hpp"

@interface MasterSystemVDPTests : XCTestCase
@end

@implementation MasterSystemVDPTests

- (void)setUp {
	[super setUp];
}

- (void)testLineInterrupt {
	TI::TMS::TMS9918 vdp(TI::TMS::Personality::SMSVDP);

	// Disable end-of-frame interrupts, enable line interrupts.
	vdp.write(1, 0x00);
	vdp.write(1, 0x81);

	vdp.write(1, 0x10);
	vdp.write(1, 0x80);

	// Set a line interrupt to occur in five lines.
	vdp.write(1, 5);
	vdp.write(1, 0x8a);

	// Get time until interrupt.
	auto time_until_interrupt = vdp.get_next_sequence_point().as_integral() - 1;

	// Check that an interrupt is now scheduled.
	NSAssert(time_until_interrupt != HalfCycles::max().as_integral() - 1, @"No interrupt scheduled");
	NSAssert(time_until_interrupt > 0, @"Interrupt is scheduled in the past");

	// Check interrupt flag isn't set prior to the reported time.
	vdp.run_for(HalfCycles(time_until_interrupt));
	NSAssert(!vdp.get_interrupt_line(), @"Interrupt line went active early [1]");

	// Check interrupt flag is set at the reported time.
	vdp.run_for(HalfCycles(1));
	NSAssert(vdp.get_interrupt_line(), @"Interrupt line wasn't set when promised [1]");

	// Read the status register to clear interrupt status.
	vdp.read(1);
	NSAssert(!vdp.get_interrupt_line(), @"Interrupt wasn't reset by status read");

	// Check interrupt flag isn't set prior to the reported time.
	time_until_interrupt = vdp.get_next_sequence_point().as_integral() - 1;
	vdp.run_for(HalfCycles(time_until_interrupt));
	NSAssert(!vdp.get_interrupt_line(), @"Interrupt line went active early [2]");

	// Check interrupt flag is set at the reported time.
	vdp.run_for(HalfCycles(1));
	NSAssert(vdp.get_interrupt_line(), @"Interrupt line wasn't set when promised [2]");
}

- (void)testFirstLineInterrupt {
	TI::TMS::TMS9918 vdp(TI::TMS::Personality::SMSVDP);

	// Disable end-of-frame interrupts, enable line interrupts, set an interrupt to occur every line.
	vdp.write(1, 0x00);
	vdp.write(1, 0x81);

	vdp.write(1, 0x10);
	vdp.write(1, 0x80);

	vdp.write(1, 0);
	vdp.write(1, 0x8a);

	// Advance to outside of the counted area.
	while(vdp.get_current_line() < 200) vdp.run_for(Cycles(228));

	// Clear the pending interrupt and ask about the next one (i.e. the first one).
	vdp.read(1);
	auto time_until_interrupt = vdp.get_next_sequence_point().as_integral() - 1;

	// Check that an interrupt is now scheduled.
	NSAssert(time_until_interrupt != HalfCycles::max().as_integral() - 1, @"No interrupt scheduled");
	NSAssert(time_until_interrupt > 0, @"Interrupt is scheduled in the past");

	// Check interrupt flag isn't set prior to the reported time.
	vdp.run_for(HalfCycles(time_until_interrupt));
	NSAssert(!vdp.get_interrupt_line(), @"Interrupt line went active early");

	// Check interrupt flag is set at the reported time.
	vdp.run_for(HalfCycles(1));
	NSAssert(vdp.get_interrupt_line(), @"Interrupt line wasn't set when promised");
}

- (void)testInterruptPrediction {
	TI::TMS::TMS9918 vdp(TI::TMS::Personality::SMSVDP);

	for(int c = 0; c < 256; ++c) {
		for(int with_eof = (c < 192) ? 0 : 1; with_eof < 2; ++with_eof) {
			// Enable or disable end-of-frame interrupts as required.
			vdp.write(1, with_eof ? 0x20 : 0x00);
			vdp.write(1, 0x81);

			// Enable line interrupts.
			vdp.write(1, 0x10);
			vdp.write(1, 0x80);

			// Set the line interrupt timing as desired.
			vdp.write(1, c);
			vdp.write(1, 0x8a);

			// Now run through an entire frame...
			int half_cycles = 262*228*2;
			auto last_time_until_interrupt = vdp.get_next_sequence_point().as_integral();
			while(half_cycles--) {
				// Validate that an interrupt happened if one was expected, and clear anything that's present.
				NSAssert(vdp.get_interrupt_line() == (last_time_until_interrupt == HalfCycles::max().as_integral()), @"Unexpected interrupt state change; expected %d but got %d; position %d %d @ %d", (last_time_until_interrupt == 0), vdp.get_interrupt_line(), c, with_eof, half_cycles);

				if(vdp.get_interrupt_line()) {
					vdp.read(1);
					last_time_until_interrupt = 0;
				}

				vdp.run_for(HalfCycles(1));

				// Get the time until interrupt.
				auto time_until_interrupt = vdp.get_next_sequence_point().as_integral();
				NSAssert(time_until_interrupt != HalfCycles::max().as_integral() || vdp.get_interrupt_line(), @"No interrupt scheduled; position %d %d @ %d", c, with_eof, half_cycles);
				NSAssert(time_until_interrupt >= 0, @"Interrupt is scheduled in the past; position %d %d @ %d", c, with_eof, half_cycles);

				if(last_time_until_interrupt > 1) {
					NSAssert(
						time_until_interrupt == (last_time_until_interrupt - 1),
						@"Discontinuity found in interrupt prediction; from %@ to %@; position %d %d @ %d",
						@(last_time_until_interrupt), @(time_until_interrupt), c, with_eof, half_cycles);
				}
				last_time_until_interrupt = time_until_interrupt;
			}
		}
	}
}

- (void)testTimeUntilLine {
	TI::TMS::TMS9918 vdp(TI::TMS::Personality::SMSVDP);

	auto time_until_line = vdp.get_time_until_line(-1).as_integral();
	for(int c = 0; c < 262*228*5; ++c) {
		vdp.run_for(HalfCycles(1));

		const auto time_remaining_until_line = vdp.get_time_until_line(-1).as_integral();
		--time_until_line;
		if(time_until_line) {
			NSAssert(
				time_remaining_until_line == time_until_line,
				@"Discontinuity found in distance-to-line prediction; expected %@ but got %@",
				@(time_until_line), @(time_remaining_until_line));
		}
		time_until_line = time_remaining_until_line;
	}
}

@end
