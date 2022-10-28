//
//  m68kDecoderTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 18/04/2022.
//  Copyright 2022 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../InstructionSets/M68k/Decoder.hpp"

using namespace InstructionSet::M68k;

@interface M68000DecoderTests : XCTestCase
@end

namespace {

template <Model model> void generate() {
	printf("{\n");
	Predecoder<model> decoder;
	for(int instr = 0; instr < 65536; instr++) {
		printf("\t\"%04x\": \"", instr);

		const auto found = decoder.decode(uint16_t(instr));
		printf("%s\"", found.to_string(instr).c_str());

		if(instr != 0xffff) printf(",");
		printf("\n");
	}
	printf("}\n");
}

template <Model model> void test(NSString *filename, Class cls) {
	NSData *const testData =
		[NSData dataWithContentsOfURL:
			[[NSBundle bundleForClass:cls]
				URLForResource:filename
				withExtension:@"json"
				subdirectory:@"68000 Decoding"]];

	NSDictionary<NSString *, NSString *> *const decodings = [NSJSONSerialization JSONObjectWithData:testData options:0 error:nil];
	XCTAssertNotNil(decodings);

	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];

	Predecoder<model> decoder;
	for(int instr = 0; instr < 65536; instr++) {
		NSString *const instrName = [NSString stringWithFormat:@"%04x", instr];
		NSString *const expected = decodings[instrName];
		XCTAssertNotNil(expected);

		const auto found = decoder.decode(uint16_t(instr));

		NSString *const instruction = [NSString stringWithUTF8String:found.to_string(instr).c_str()];
		if(![instruction isEqualToString:expected]) {
			[failures addObject:[NSString stringWithFormat:@"%@ should decode as %@; got %@", instrName, expected, instruction]];
		}
	}

	XCTAssertEqual([failures count], 0);
	if([failures count]) {
		NSLog(@"%@", failures);
	}
}

}

@implementation M68000DecoderTests

- (void)test68000 {
	test<Model::M68000>(@"68000ops", [self class]);
}

- (void)test68010 {
	test<Model::M68010>(@"68010ops", [self class]);
}

/*
	TODO: generate full new reference JSONs for tests below here.
	For now these are here for manual verification of the diffs.
*/

- (void)test68020 {
//	generate<Model::M68020>();
	test<Model::M68020>(@"68020ops", [self class]);
}

@end
