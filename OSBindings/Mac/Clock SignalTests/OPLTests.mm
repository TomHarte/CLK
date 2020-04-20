//
//  OPLTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 14/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "OPL2.hpp"
#include <cmath>

@interface OPLTests: XCTestCase
@end

@implementation OPLTests {
}

// MARK: - Table tests

- (void)testSineLookup {
	for(int c = 0; c < 1024; ++c) {
		const auto logSin = Yamaha::OPL::negative_log_sin(c);
		const auto level = Yamaha::OPL::power_two(logSin);

		double fl_angle = double(c) * M_PI / 512.0;
		double fl_level = sin(fl_angle);

		XCTAssertLessThanOrEqual(fabs(fl_level - double(level) / 4096.0), 0.01, "Sine varies by more than 0.01 at angle %d", c);
	}
}

// MARK: - Two-operator FM tests

- (void)compareFMTo:(NSArray *)knownGood atAttenuation:(int)attenuation {
	Yamaha::OPL::Operator modulator, carrier;
	Yamaha::OPL::Channel channel;

	// Set: AM = 0, PM = 0, EG = 1, KR = 0, MUL = 0
	modulator.set_am_vibrato_hold_sustain_ksr_multiple(0x20);
	carrier.set_am_vibrato_hold_sustain_ksr_multiple(0x20);

	// Set: KL = 0, TL = 0
	modulator.set_scaling_output(attenuation);
	carrier.set_scaling_output(0);

	// Set: waveform = 0.
	modulator.set_waveform(0);
	carrier.set_waveform(0);

	// Set FB = 0, use FM synthesis.
	channel.set_feedback_mode(1);

	// Set: AR = 15, DR = 0.
	modulator.set_attack_decay(0xf0);
	carrier.set_attack_decay(0xf0);

	// Set: SL = 0, RR = 15.
	modulator.set_sustain_release(0x0f);
	carrier.set_sustain_release(0x0f);

	// Set 16384 samples for 1 period, and key-on.
	channel.set_frequency_low(0x40);
	channel.set_9bit_frequency_octave_key_on(0x10);

	// Check one complete cycle of samples.
	NSEnumerator *goodValues = [knownGood objectEnumerator];
	for(int c = 0; c < 16384; ++c) {
		const int generated = channel.update(&modulator, &carrier);
		const int known = [[goodValues nextObject] intValue];
		XCTAssertLessThanOrEqual(abs(generated - known), 10, "FM synthesis varies by more than 10 at sample %d of attenuation %d", c, attenuation);
	}
}

- (void)testFM {
	// The following have been verified by sight against
	// the images at https://www.smspower.org/Development/RE10
	// as "close enough". Sadly the raw data isn't given, so
	// that's the best as I can do. Fingers crossed!

	NSURL *const url = [[NSBundle bundleForClass:[self class]] URLForResource:@"fm"  withExtension:@"json"];
	NSArray *const parent = [NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfURL:url] options:0 error:nil];

	for(int c = 0; c < 64; ++c) {
		[self compareFMTo:parent[c] atAttenuation:c];
	}
}

// MARK: - ADSR tests

- (void)testADSR {
	Yamaha::OPL::Operator test_operator;
	Yamaha::OPL::OperatorState test_state;

	test_operator.set_attack_decay(0x88);
	test_operator.set_sustain_release(0x88);

	// While key is off, output level should remain at 0.
	for(int c = 0; c < 1024; ++c) {
		test_operator.update(test_state, false, 0, 0, 0);
		XCTAssertGreaterThanOrEqual(test_state.level(), 0);
	}

	// Set key on...
	for(int c = 0; c < 4096; ++c) {
		test_operator.update(test_state, true, 0, 0, 0);
		NSLog(@"%d", test_state.level());
	}
}

@end
