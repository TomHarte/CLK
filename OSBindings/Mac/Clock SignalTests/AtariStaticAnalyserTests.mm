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

#define Record(sha, model, uses) sha : [AtariROMRecord recordWithPagingModel:StaticAnalyser::Atari2600PagingModel::model usesSuperchip:uses],
static NSDictionary<NSString *, AtariROMRecord *> *romRecordsBySHA1 = @{
	Record(@"58dbcbdffbe80be97746e94a0a75614e64458fdc", None, NO)		// 4kraVCS
	Record(@"9967a76efb68017f793188f691159f04e6bb4447", None, NO)		// 'X'Mission
	Record(@"21d983f2f52b84c22ecae84b0943678ae2c31c10", None, NO)		// 3d Tic-Tac-Toe
	Record(@"d7c62df8300a68b21ce672cfaa4d0f2f4b3d0ce1", Atari32k, NO)	// Acid Drop
	Record(@"924ca836aa08eeffc141d487ac6b9b761b2f8ed5", None, NO)		// Action Force
	Record(@"e07e48d463d30321239a8acc00c490f27f1f7422", None, NO)		// Adventure
};
#undef Record

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

		// grab the ROM record
		AtariROMRecord *romRecord = [self romRecordForSHA1:sha1];
		if(!romRecord)
		{
			NSLog(@"No record for %@ with SHA1 %@", testFile, sha1);
			continue;
		}

		// assert equality
		XCTAssert(targets.front().atari.paging_model == romRecord.pagingModel, @"%@", testFile);
		XCTAssert(targets.front().atari.uses_superchip == romRecord.usesSuperchip, @"%@", testFile);
	}
}

@end
