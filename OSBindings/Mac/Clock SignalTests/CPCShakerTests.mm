//
//  CPCShakerTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include "CSL.hpp"

//
// Runs a local capture of the test cases found at https://shaker.logonsystem.eu
//
@interface CPCShakerTests : XCTestCase
@end

@implementation CPCShakerTests {
}

- (void)testCSLPath:(NSString *)path name:(NSString *)name {
	using namespace Storage::Automation;
	const auto steps = CSL::parse([[path stringByAppendingPathComponent:name] UTF8String]);
	NSLog(@"%@ / %@", path, name);
}

- (void)testModulePath:(NSString *)path name:(NSString *)name {
	NSString *basePath =
		[[NSBundle bundleForClass:[self class]]
			pathForResource:@"Shaker"
			ofType:nil];
	[self testCSLPath:[basePath stringByAppendingPathComponent:path] name:name];
}

- (void)testModuleA {	[self testModulePath:@"MODULE A" name:@"SHAKE26A-0.CSL"];	}
- (void)testModuleB {	[self testModulePath:@"MODULE B" name:@"SHAKE26B-0.CSL"];	}
- (void)testModuleC {	[self testModulePath:@"MODULE C" name:@"SHAKE26C-0.CSL"];	}
- (void)testModuleD {	[self testModulePath:@"MODULE D" name:@"SHAKE26D-0.CSL"];	}
- (void)testModuleE {	[self testModulePath:@"MODULE E" name:@"SHAKE26E-0.CSL"];	}

@end
