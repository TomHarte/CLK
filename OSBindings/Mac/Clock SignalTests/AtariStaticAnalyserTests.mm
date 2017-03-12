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
	Record(@"d7c62df8300a68b21ce672cfaa4d0f2f4b3d0ce1", Atari16k, NO)	// Acid Drop
	Record(@"924ca836aa08eeffc141d487ac6b9b761b2f8ed5", None, NO)		// Action Force
	Record(@"e07e48d463d30321239a8acc00c490f27f1f7422", None, NO)		// Adventure
	Record(@"03a495c7bfa0671e24aa4d9460d232731f68cb43", None, NO)		// Adventures of Tron
	Record(@"6e420544bf91f603639188824a2b570738bb7e02", None, NO)		// Adventures On GX12.a26
	Record(@"3b02e7dacb418c44d0d3dc77d60a9663b90b0fbc", None, NO)		// Air Raid
	Record(@"29f5c73d1fe806a4284547274dd73f9972a7ed70", None, NO)		// Air Raiders
	Record(@"af5b9f33ccb7778b42957da4f20f2bc000992366", None, NO)		// Air-Sea Battle
	Record(@"0376c242819b785310b8af43c03b1d1156bd5f02", None, NO)		// Airlock
	Record(@"fb870ec3d51468fa4cf40e0efae9617e60c1c91c", None, NO)		// AKA Space Adventure
	Record(@"01d99bf307262825db58631e8002dd008a42cb1e", None, NO)		// Alien
	Record(@"a1f660827ce291f19719a5672f2c5d277d903b03", Atari8k, NO)	// Alpha Beam with Ernie
	Record(@"b89a5ac6593e83fbebee1fe7d4cec81a7032c544", None, NO)		// Amidar
	Record(@"ac58ac94ceab78725a1182cc7b907376c011b0c8", None, NO)		// Angriff der Luftflotten
	Record(@"7d132ab776ff755b86bf4f204165aa54e9e1f1cf", Atari8k, NO)	// Aquaventure
	Record(@"9b6a54969240baf64928118741c3affee148d721", None, NO)		// Armor Ambush
	Record(@"8c249e9eaa83fc6be16039f05ec304efdf987beb", Atari8k, NO)	// Artillery Duel
	Record(@"0c03eba97df5178eec5d4d0aea4a6fe2f961c88f", None, NO)		// Assault
	Record(@"1a094f92e46a8127d9c29889b5389865561c0a6f", Atari8k, NO)	// Asterix (NTSC)
	Record(@"f14408429a911854ec76a191ad64231cc2ed7d11", Atari8k, NO)	// Asterix (PAL)
	Record(@"8f4a00cb4ab6a6f809be0e055d97e8fe17f19e7d", None, NO)		// Asteroid Fire
	Record(@"8423f99092b454aed89f89f5d7da658caf7af016", Atari8k, NO)	// Asteroids
	Record(@"b850bd72d18906d9684e1c7251cb699588cbcf64", None, NO)		// Astroblast
	Record(@"d1563c24208766cf8d28de7af995021a9f89d7e1", None, NO)		// Atari Video Cube
	Record(@"f4e838de9159c149ac080ab85e4f830d5b299963", None, NO)		// Atlantis II
	Record(@"c6b1dcdb2f024ab682316db45763bacc6949c33c", None, NO)		// Atlantis
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
