//
//  ArchimedesStaticAnalyserTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 20/05/2024.
//  Copyright 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#import <CommonCrypto/CommonDigest.h>
#include "Analyser/Static/StaticAnalyser.hpp"
#include "Analyser/Static/Acorn/Target.hpp"

static NSString *archimedesDiskPath = @"/Users/thomasharte/Library/Mobile Documents/com~apple~CloudDocs/Soft/Archimedes";

static NSDictionary<NSString *, NSString *> *mainProgramsBySHA1 = @{
	@"371b30787a782cb1fe6cb6ad2217a832a06e1e96": @"!TimeZone",
	@"3459adef724e2cd6f3681050a9ce47394231b4f9": @"!Talisman",
	@"3022e18d47ed0fc14b09c18caff3fc0ac1f4edff": @"!StarTrade",
	@"252bfde8d602fe171e0657fa3f9dfeba1803e6eb": @"!Blowpipe",
	@"e3c32b8cbd3cd31cbca93e5a45b94e7f8058b8f7": @"Zelanites.!Zelanites",
	@"2e1cb15cde588e22f50518b6ffa47a8df89b14c0": @"!Fire_Ice",
	@"069592c0b90a0b9112daf014b7e19b4a51f9653b": @"!UIM",
	@"14c3785b3bc3f7e2d4a81e92ff06e11656e6b76c": @"!UIM",
	@"93b67127286d861e4df31cac27e78e623a1e852f": @"!FineRacer",
	@"53f95c169bbe9cfa7252d90d6181ced31086f1a5": @"!adventure",
	@"4168bb21f6df0976ce227a20f9fa4eb240289f3b": @"!BigBang",
	@"8fcad522ea22b75b393ceb334cfef3f324b248ee": @"!E-TYPE",
	@"8ca4289ac423d4878129cb17d6177123b321108f": @"!StrtWrite",
	@"4f92efecfc1e3a510a816f570ccb7082f0154e37": @"!HeroQuest",
	@"9bd6d2514c04ce02fcf8ef214815229b28be56d8": @"!adventure",
	@"d3493850e8ed91ae0a55a53866139781ad65e63d": @"!Nebulus",
	@"ba655bd8936859a33bab5fde447e33486c3b0d3e": @"!Attack",
	@"a6502faf15ddb4acaed2ca859cedc1225e7fa762": @"!Wolf",
//	@"04f588f87facd507e043b06f512e9bdb6fe996c0":	// TODO: should decline to pick.

	// Various things that are not the first disk.
	@"2cff99237837e2291b845eb63977362ad9b4f040": @"",
	@"3615bcb8a953fbba3d56a956243341a022208101": @"",
	@"03672244691b292d6b4816aa592b312ea6297b22": @"",
	@"b7139d9bd927b8e4d933fd8aa3080a7249117495": @"",
	@"66a82651f86d9cf0aa5b54c55bcaa8fefd3901da": @"",
	@"c3d3cd9e28f5e7499fd70057f820c75219538c69": @"",
	@"81bfd4ab92c538f5b15ad64bba625aac2ffb243d": @"",
	@"39318695b6e64c9d7270f2b6d8213a7d4b0b0c43": @"",
};
#undef Record

@interface ArchimedesStaticAnalyserTests : XCTestCase
@end

@implementation ArchimedesStaticAnalyserTests

- (void)testADFs {
	for(NSString *testFile in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:archimedesDiskPath error:nil]) {
		NSString *fullPath = [archimedesDiskPath stringByAppendingPathComponent:testFile];

		// Compute file SHA1.
		NSData *fileData = [NSData dataWithContentsOfFile:fullPath];
		uint8_t sha1Bytes[CC_SHA1_DIGEST_LENGTH];
		CC_SHA1([fileData bytes], (CC_LONG)[fileData length], sha1Bytes);
		NSMutableString *const sha1 = [[NSMutableString alloc] init];
		for(int c = 0; c < CC_SHA1_DIGEST_LENGTH; c++) [sha1 appendFormat:@"%02x", sha1Bytes[c]];

		// Get analysed target and correct answer per list above.
		auto targets = Analyser::Static::GetTargets([fullPath UTF8String]);
		NSString *const mainProgram = mainProgramsBySHA1[sha1];
		if(!mainProgram) {
			NSLog(@"Not checking %@ with SHA1 %@", testFile, sha1);
			continue;
		}

		if(![mainProgram length]) {
			continue;
		}

		// Test equality.
		auto *const target = dynamic_cast<Analyser::Static::Acorn::ArchimedesTarget *>(targets.front().get());
		XCTAssert(target != nullptr);
		XCTAssert(target->main_program == std::string([mainProgram UTF8String]), @"%@; should be %@, is %s", testFile, mainProgram, target->main_program.c_str());
	}
}

@end
