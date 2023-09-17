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
#include <sstream>
#include <fstream>

#include "NSData+dataWithContentsOfGZippedFile.h"

#include "../../../InstructionSets/x86/Decoder.hpp"

namespace {

// The tests themselves are not duplicated in this repository;
// provide their real path here.
constexpr char TestSuiteHome[] = "/Users/thomasharte/Projects/ProcessorTests/8088/v1";

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];
	NSSet *allowList = nil;
//		[[NSSet alloc] initWithArray:@[
//			@"00.json.gz",
//		]];

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *) {
		if(allowList && ![allowList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
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

	// Build a vector of the instruction; this makes manual step debugging easier.
	NSArray<NSNumber *> *encoding = test[@"bytes"];
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}

	const auto decoded = decoder.decode(data.data(), data.size());
	XCTAssert(
		decoded.first == [encoding count],
		"Wrong length of instruction decoded for %@ — decoded %d rather than %lu",
			test[@"name"],
			decoded.first,
			(unsigned long)[encoding count]
	);

	if(decoded.first != [encoding count]) {
		NSMutableString *hexInstruction = [[NSMutableString alloc] init];
		for(uint8_t byte: data) {
			[hexInstruction appendFormat:@"%02x ", byte];
		}
		NSLog(@"Instruction was %@", hexInstruction);

		// Repeat the decoding, for ease of debugging.
		Decoder straw_man;
		straw_man.decode(data.data(), data.size());
		return false;
	}

	// Form string version, compare.
	std::string operation;

	using Operation = InstructionSet::x86::Operation;
	int operands = 0;
	switch(decoded.second.operation) {
		case Operation::SUB:	operation += "sub";	operands = 2;	break;
		default: break;
	}

	auto to_string = [] (InstructionSet::x86::DataPointer pointer, const auto &instruction) -> std::string {
		std::string operand;

		using Source = InstructionSet::x86::Source;
		const bool is_byte_operation = instruction.operation_size() == InstructionSet::x86::DataSize::Byte;
		switch(pointer.source()) {
			case Source::eAX:		return is_byte_operation ? "al" : "ax";
			case Source::eCX:		return is_byte_operation ? "cl" : "cx";
			case Source::eDX:		return is_byte_operation ? "dl" : "dx";
			case Source::eBX:		return is_byte_operation ? "bl" : "bx";
			case Source::eSPorAH:	return is_byte_operation ? "ah" : "sp";
			case Source::eBPorCH:	return is_byte_operation ? "ch" : "bp";
			case Source::eSIorDH:	return is_byte_operation ? "dh" : "si";
			case Source::eDIorBH:	return is_byte_operation ? "bh" : "di";

			case Source::ES:	return "es";
			case Source::CS:	return "cs";
			case Source::SS:	return "ss";
			case Source::DS:	return "ds";
			case Source::FS:	return "fd";
			case Source::GS:	return "gs";

			case Source::Immediate:
				return (std::stringstream() <<  std::setfill('0') << std::setw(4) << std::uppercase << std::hex << instruction.operand() << 'h').str();

			default: break;
		}
		switch(pointer.index()) {
			default: break;
			case Source::eAX:	operand += "ax";	break;
		}

		return operand;
	};

	if(operands > 1) {
		operation += " ";
		operation += to_string(decoded.second.destination(), decoded.second);
		operation += ",";
	}
	if(operands > 0) {
		operation += " ";
		operation += to_string(decoded.second.source(), decoded.second);
	}

	XCTAssertEqualObjects([NSString stringWithUTF8String:operation.c_str()], test[@"name"]);

	return true;
}

- (void)testDecoding {
	NSMutableSet<NSString *> *failures = [[NSMutableSet alloc] init];
	NSArray<NSString *> *testFiles = [self testFiles];

	for(NSString *file in testFiles) {
		NSData *data = [NSData dataWithContentsOfGZippedFile:file];
		NSArray<NSDictionary *> *testsInFile = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
		NSUInteger successes = 0;
		for(NSDictionary *test in testsInFile) {
			// A single failure per instruction is fine.
			if(![self applyDecodingTest:test]) {
				[failures addObject:file];
				break;
			}
			++successes;
		}
		if(successes != [testsInFile count]) {
			NSLog(@"Failed after %ld successes", successes);
		}
	}

	NSLog(@"%ld failures out of %ld tests: %@", failures.count, testFiles.count, [[failures allObjects] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]);
}

@end
