//
//  8088Tests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 13/09/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include <iostream>
#include <fstream>

#include "NSData+dataWithContentsOfGZippedFile.h"

#include "../../../InstructionSets/x86/Decoder.hpp"

namespace {

// The tests themselves are not duplicated in this repository;
// provide their real path here.
constexpr char TestSuiteHome[] = "/Users/tharte/Projects/ProcessorTests/8088/v1";

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *) {
		return [evaluatedObject hasSuffix:@"json.gz"];
	}]];

	NSMutableArray<NSString *> *fullPaths = [[NSMutableArray alloc] init];
	for(NSString *file in files) {
		[fullPaths addObject:[path stringByAppendingPathComponent:file]];
	}

	return fullPaths;
}

- (bool)applyDecodingTest:(NSDictionary *)test {
	using Decoder = InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086>;
	Decoder decoder;

	NSArray<NSNumber *> *encoding = test[@"bytes"];
	std::pair<int, Decoder::InstructionT> stage;
	for(NSNumber *number in encoding) {
		const uint8_t next = [number intValue];
		stage = decoder.decode(&next, 1);
		if(stage.first > 0) {
			break;
		}
	}

	XCTAssert(
		stage.first == [encoding count],
		"Wrong length of instruction decoded for %@ — decoded %d rather than %lu",
			test[@"name"],
			stage.first,
			(unsigned long)[encoding count]
	);

	if(stage.first != [encoding count]) {
		return false;
	}
	// TODO: form string version, compare.

	return true;
}

- (void)testDecoding {
	for(NSString *file in [self testFiles]) {
		NSData *data = [NSData dataWithContentsOfGZippedFile:file];
		NSArray<NSDictionary *> *testsInFile = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
		NSUInteger successes = 0;
		for(NSDictionary *test in testsInFile) {
			// A single failure per instruction is fine.
			if(![self applyDecodingTest:test]) {
				break;
			}
			++successes;
		}
		if(successes != [testsInFile count]) {
			NSLog(@"Failed after %ld successes", successes);
		}
	}
}

@end
