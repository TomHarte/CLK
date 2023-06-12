//
//  DispatcherTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 12/06/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "RangeDispatcher.hpp"

#include <array>
#include <cassert>

@interface DispatcherTests : XCTestCase
@end

@implementation DispatcherTests {
}

- (void)setUp {
}

- (void)tearDown {
}

struct DoStep {
	static constexpr int max = 100;
	template <int n> void perform(int, int) {
		assert(n < max);
		performed[n] = true;
	}
	std::array<bool, max> performed{};
};

- (void)testPoints {
	DoStep stepper;

	Reflection::RangeDispatcher<DoStep>::dispatch(stepper, 0, 10);
	for(size_t c = 0; c < stepper.performed.size(); c++) {
		XCTAssert(stepper.performed[c] == (c < 10));
	}

	Reflection::RangeDispatcher<DoStep>::dispatch(stepper, 29, 100000);
	for(size_t c = 0; c < stepper.performed.size(); c++) {
		XCTAssert(stepper.performed[c] == (c < 10) || (c >= 29));
	}
}

- (void)testRanges {
}

@end
