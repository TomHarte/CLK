//
//  OPLTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 14/04/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "OPL2.hpp"

@interface OPLTests: XCTestCase
@end

@implementation OPLTests {
}

- (void)testADSR {
	Yamaha::OPL::Operator test_operator;
	Yamaha::OPL::OperatorState test_state;

	test_operator.set_attack_decay(0x88);
	test_operator.set_sustain_release(0x88);

	// While key is off, ADSR attenuation should remain above 511.
	for(int c = 0; c < 1024; ++c) {
		test_operator.update(test_state, false, 0, 0);
		XCTAssertGreaterThanOrEqual(test_state.attenuation, 511);
	}

	// Set key on...
	for(int c = 0; c < 4096; ++c) {
		test_operator.update(test_state, true, 0, 0);
		NSLog(@"%d", test_state.attenuation);
	}

}

@end
