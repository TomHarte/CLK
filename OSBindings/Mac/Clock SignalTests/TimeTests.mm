//
//  TimeTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 24/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "Storage.hpp"

@interface TimeTests : XCTestCase
@end

@implementation TimeTests

- (void)testHalf
{
	Storage::Time half(0.5f);
	XCTAssert(half == Storage::Time(1, 2), @"0.5 should be converted to 1/2");
}

- (void)testTwenty
{
	Storage::Time twenty(20.0f);
	XCTAssert(twenty == Storage::Time(20, 1), @"20.0 should be converted to 20/1");
}

- (void)testTooSmallFloat
{
	float original = 1.0f / powf(2.0f, 25.0f);
	Storage::Time time(original);
	XCTAssert(time == Storage::Time(0), @"Numbers too small to be represented should be 0");
}

@end
