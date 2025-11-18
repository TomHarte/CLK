//
//  SIDTests.mm
//  Clock SignalTests
//
//  Created by Thomas Harte on 11/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "Components/SID/SID.hpp"

@interface SIDTests : XCTestCase
@end

@implementation SIDTests

- (void)testOscillator {
	MOS::SID::Voice prior;
	MOS::SID::Voice voice;
	const uint32_t pulse_width = 0x02'3;

	voice.oscillator.pitch = 0x00'1000'00;
	voice.oscillator.pulse_width = pulse_width << 20;
	voice.oscillator.reset_phase();

	int c = 0;

	// Run for first half of a cycle.
	while(!voice.oscillator.did_raise_b23()) {
		// Force envelope.
		voice.adsr.envelope = 255;

		// Test sawtooth.
		voice.set_control(0x20);
		XCTAssertEqual(voice.output(prior), c);

		// Test triangle.
		voice.set_control(0x10);
		XCTAssertEqual(voice.output(prior), c << 1);

		// Test pulse.
		voice.set_control(0x40);
		XCTAssertEqual(voice.output(prior), (c < pulse_width) ? 0 : 4095);

		// Advance.
		voice.update();
		++c;
	}

	// B23 should go up halfway through the 12-bit range.
	XCTAssertEqual(c, 2048);

	// Run for second half of a cycle.
	while(c < 4096) {
		// Force envelope.
		voice.adsr.envelope = 255;

		// Test sawtooth.
		voice.set_control(0x20);
		XCTAssertEqual(voice.output(prior), c);

		// Test triangle.
		voice.set_control(0x10);
		XCTAssertEqual(voice.output(prior), 4095 - ((c << 1) & 4095));

		// Test pulse.
		voice.set_control(0x40);
		XCTAssertEqual(voice.output(prior), (c <= pulse_width) ? 0 : 4095);

		// Advance.
		voice.update();
		++c;

		XCTAssert(!voice.oscillator.did_raise_b23());
	}

	// Check that B23 doesn't false rise again.
	voice.update();
	XCTAssert(!voice.oscillator.did_raise_b23());
}

@end
