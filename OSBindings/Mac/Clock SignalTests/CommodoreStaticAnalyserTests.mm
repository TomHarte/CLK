//
//  AtariStaticAnalyserTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/03/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#import <CommonCrypto/CommonDigest.h>
#include "../../../Analyser/Static/StaticAnalyser.hpp"
#include "../../../Analyser/Static/Commodore/Target.hpp"

// This test runs through a whole bunch of files somewhere on disk. These files are not included in the repository
// because they are not suitably licensed. So this path is specific to my local system, at the time I happen to be
// writing these tests. Update in the future, as necessary.
static constexpr const char *const plus4Path =
	"/Users/thomasharte/Library/Mobile Documents/com~apple~CloudDocs/Soft/C16+4";
static constexpr const char *const vic20Path =
	"/Users/thomasharte/Library/Mobile Documents/com~apple~CloudDocs/Soft/Vic-20";

@interface CommodoreStaticAnalyserTests : XCTestCase
@end

struct HitRate {
	int files = 0;
	int matches = 0;

	HitRate &operator += (const HitRate &rhs) {
		files += rhs.files;
		matches += rhs.matches;
		return *this;
	}
};

@implementation CommodoreStaticAnalyserTests

- (HitRate)hitRateBeneathPath:(NSString *)path forMachine:(Analyser::Machine)machine {
	HitRate hits{};

	NSDirectoryEnumerator<NSString *> *enumerator = [[NSFileManager defaultManager] enumeratorAtPath:path];
	while(NSString *diskItem = [enumerator nextObject]) {
		const NSString *type = [[enumerator fileAttributes] objectForKey:NSFileType];
		if(![type isEqual:NSFileTypeRegular]) {
			continue;
		}

		NSLog(@"%@", diskItem);
		const auto list = Analyser::Static::GetTargets([path stringByAppendingPathComponent:diskItem].UTF8String);
		if(list.empty()) {
			continue;
		}

		++hits.files;
		if(list.size() != 1) {
			continue;
		}

		const auto &first = *list.begin();
		hits.matches += first->machine == machine;

//		if(!(hits.files % 100)) {
			NSLog(@"Currently %d in %d, i.e. %0.2f",
				hits.matches, hits.files, float(hits.matches) / float(hits.files));
//		}
	}

	return hits;
}

- (void)testPlus4 {
	const auto hitRate =
		[self hitRateBeneathPath:[NSString stringWithUTF8String:plus4Path] forMachine:Analyser::Machine::Plus4];
	NSLog(@"Got hit rate of %d in %d, i.e. %0.2f",
		hitRate.matches, hitRate.files, float(hitRate.matches) / float(hitRate.files));
}

- (void)testVic20 {
	const auto hitRate =
		[self hitRateBeneathPath:[NSString stringWithUTF8String:vic20Path] forMachine:Analyser::Machine::Vic20];
	NSLog(@"Got hit rate of %d in %d, i.e. %0.2f",
		hitRate.matches, hitRate.files, float(hitRate.matches) / float(hitRate.files));
}

@end
