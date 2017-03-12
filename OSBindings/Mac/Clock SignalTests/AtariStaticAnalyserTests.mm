//
//  AtariStaticAnalyserTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 11/03/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#import <CommonCrypto/CommonDigest.h>
#include "../../../StaticAnalyser/StaticAnalyser.hpp"

@interface AtariROMRecord : NSObject
@property(nonatomic, readonly) StaticAnalyser::Atari2600PagingModel pagingModel;
@property(nonatomic, readonly) BOOL usesSuperchip;
+ (instancetype)recordWithPagingModel:(StaticAnalyser::Atari2600PagingModel)pagingModel usesSuperchip:(BOOL)usesSuperchip;
@end

@implementation AtariROMRecord
+ (instancetype)recordWithPagingModel:(StaticAnalyser::Atari2600PagingModel)pagingModel usesSuperchip:(BOOL)usesSuperchip
{
	AtariROMRecord *record = [[AtariROMRecord alloc] init];
	record->_pagingModel = pagingModel;
	record->_usesSuperchip = usesSuperchip;
	return record;
}
@end

static NSDictionary<NSString *, AtariROMRecord *> *romRecordsBySHA1 = @{
};

@interface AtariStaticAnalyserTests : XCTestCase
@end

@implementation AtariStaticAnalyserTests

- (AtariROMRecord *)romRecordForSHA1:(NSString *)sha1
{
	return romRecordsBySHA1[sha1];
}

- (void)testAtariROMS
{
	NSString *basePath = [[[NSBundle bundleForClass:[self class]] resourcePath] stringByAppendingPathComponent:@"Atari ROMs"];
	for(NSString *testFile in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:basePath error:nil])
	{
		NSString *fullPath = [basePath stringByAppendingPathComponent:testFile];

		// get a SHA1 for the file
		NSData *fileData = [NSData dataWithContentsOfFile:fullPath];
		uint8_t sha1Bytes[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1([fileData bytes], (CC_LONG)[fileData length], sha1Bytes);
		NSMutableString *sha1 = [[NSMutableString alloc] init];
		for(int c = 0; c < CC_SHA1_DIGEST_LENGTH; c++) [sha1 appendFormat:@"%02x", sha1Bytes[c]];

		// get an analysis of the file
		std::list<StaticAnalyser::Target> targets = StaticAnalyser::GetTargets([fullPath UTF8String]);

		AtariROMRecord *romRecord = [self romRecordForSHA1:sha1];
		if(!romRecord)
		{
			NSLog(@"No record for %@ with SHA1 %@", testFile, sha1);
			continue;
		}
	}
}

@end
