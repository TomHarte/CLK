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

- (void)testFloat
{
	Storage::Time half(0.5f);
	XCTAssert(half == Storage::Time(1, 2), @"0.5 should be converted to 1/2");
}

@end
