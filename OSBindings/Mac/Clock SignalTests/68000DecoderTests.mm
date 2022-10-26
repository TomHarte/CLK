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

template <Model model> void test(NSString *filename, Class cls) {
	NSData *const testData =
		[NSData dataWithContentsOfURL:
			[[NSBundle bundleForClass:cls]
				URLForResource:filename
				withExtension:@"json"
				subdirectory:@"68000 Decoding"]];

	NSDictionary<NSString *, NSString *> *const decodings = [NSJSONSerialization JSONObjectWithData:testData options:0 error:nil];
	XCTAssertNotNil(decodings);

	Predecoder<model> decoder;
	for(int instr = 0; instr < 65536; instr++) {
		NSString *const instrName = [NSString stringWithFormat:@"%04x", instr];
		NSString *const expected = decodings[instrName];
		XCTAssertNotNil(expected);

		const auto found = decoder.decode(uint16_t(instr));

		NSString *const instruction = [NSString stringWithUTF8String:found.to_string(instr).c_str()];
		XCTAssertEqualObjects(instruction, expected, "%@ should decode as %@; got %@", instrName, expected, instruction);
	}
}

}

@implementation M68000DecoderTests

- (void)test68000 {
	test<Model::M68000>(@"68000ops", [self class]);
}


/*
	TODO: generate new reference JSONs for tests below here.
	For now these are here for manual verification of the diffs.
*/

- (void)test68010 {
	test<Model::M68010>(@"68000ops", [self class]);
}

- (void)test68020 {
	test<Model::M68020>(@"68000ops", [self class]);
}

@end
