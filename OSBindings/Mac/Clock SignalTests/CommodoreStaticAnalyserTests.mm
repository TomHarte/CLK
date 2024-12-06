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

#include <atomic>

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
	__block std::atomic<int> files_source = 0;
	__block std::atomic<int> matches_source = 0;
	auto &files = files_source;
	auto &matches = matches_source;

	NSDirectoryEnumerator<NSString *> *enumerator = [[NSFileManager defaultManager] enumeratorAtPath:path];

	const auto dispatch_group = dispatch_group_create();

	while(NSString *diskItem = [enumerator nextObject]) {
		const NSString *type = [[enumerator fileAttributes] objectForKey:NSFileType];
		if(![type isEqual:NSFileTypeRegular]) {
			continue;
		}

		NSString *const fullPath = [path stringByAppendingPathComponent:diskItem];
		dispatch_group_async(dispatch_group, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
			const auto list = Analyser::Static::GetTargets(fullPath.UTF8String);
			if(list.empty()) {
				return;
			}

			++files;
			if(list.size() != 1) {
				return;
			}

			const auto &first = *list.begin();
			matches += first->machine == machine;

			if(!(files % 100)) {
				NSLog(@"Currently %d in %d, i.e. %0.2f",
					matches.load(), files.load(), float(matches.load()) / float(files.load()));
			}
		});
	}

	dispatch_group_wait(dispatch_group, DISPATCH_TIME_FOREVER);
	return HitRate {
		.files = files,
		.matches = matches,
	};
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
