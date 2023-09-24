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
constexpr char TestSuiteHome[] = "/Users/tharte/Projects/ProcessorTests/8088/v1";

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];
	NSSet *allowList = nil;
//		[[NSSet alloc] initWithArray:@[
//			@"08.json.gz",
//		]];

	// Unofficial opcodes; ignored for now.
	NSSet *ignoreList =
		[[NSSet alloc] initWithArray:@[
			@"60.json.gz",		@"61.json.gz",		@"62.json.gz",		@"63.json.gz",
			@"64.json.gz",		@"65.json.gz",		@"66.json.gz",		@"67.json.gz",
			@"68.json.gz",		@"69.json.gz",		@"6a.json.gz",		@"6b.json.gz",
			@"6c.json.gz",		@"6d.json.gz",		@"6e.json.gz",		@"6f.json.gz",

			@"82.0.json.gz",	@"82.1.json.gz",	@"82.2.json.gz",	@"82.3.json.gz",
			@"82.4.json.gz",	@"82.5.json.gz",	@"82.6.json.gz",	@"82.7.json.gz",

			@"c0.json.gz",		@"c1.json.gz",		@"c8.json.gz",		@"c9.json.gz",

			@"f6.1.json.gz",	@"f7.1.json.gz",
			@"ff.7.json.gz",
		]];

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *) {
		if(allowList && ![allowList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
		if([ignoreList containsObject:[evaluatedObject lastPathComponent]]) {
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

- (NSString *)toString:(const InstructionSet::x86::Instruction<false> &)instruction abbreviateOffset:(BOOL)abbreviateOffset {
	// Form string version, compare.
	std::string operation;

	using Repetition = InstructionSet::x86::Repetition;
	switch(instruction.repetition()) {
		case Repetition::None: break;
		case Repetition::RepE: operation += "repe ";	break;
		case Repetition::RepNE: operation += "repne ";	break;
	}

	operation += to_string(instruction.operation, instruction.operation_size());

	auto to_hex = [] (int value, int digits) -> std::string {
		auto stream = std::stringstream();
		stream << std::setfill('0') << std::uppercase << std::hex << std::setw(digits);
		switch(digits) {
			case 2: stream << +uint8_t(value);	break;
			case 4: stream << +uint16_t(value);	break;
			default: stream << value;	break;
		}
		stream << 'h';
		return stream.str();
	};

	auto to_string = [&to_hex, abbreviateOffset] (InstructionSet::x86::DataPointer pointer, const auto &instruction) -> std::string {
		std::string operand;

		using Source = InstructionSet::x86::Source;
		const Source source = pointer.source<false>();
		switch(source) {
			// to_string handles all direct register names correctly.
			default:	return InstructionSet::x86::to_string(source, instruction.operation_size());

			case Source::Immediate:
				return to_hex(
					instruction.operand(),
					instruction.operation_size() == InstructionSet::x86::DataSize::Byte ? 2 : 4
				);

			case Source::DirectAddress:
			case Source::Indirect:
			case Source::IndirectNoBase: {
				std::stringstream stream;

				if(!InstructionSet::x86::mnemonic_implies_data_size(instruction.operation)) {
					stream << InstructionSet::x86::to_string(instruction.operation_size()) << ' ';
				}

				Source segment = instruction.data_segment();
				if(segment == Source::None) {
					segment = pointer.default_segment();
					if(segment == Source::None) {
						segment = Source::DS;
					}
				}
				stream << InstructionSet::x86::to_string(segment, InstructionSet::x86::DataSize::None) << ':';

				stream << '[';
				bool addOffset = false;
				switch(source) {
					default: break;
					case Source::Indirect:
						stream << InstructionSet::x86::to_string(pointer.base(), data_size(instruction.address_size()));
						stream << '+' << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
						addOffset = true;
					break;
					case Source::IndirectNoBase:
						stream << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
						addOffset = true;
					break;
					case Source::DirectAddress:
						stream << to_hex(instruction.offset(), 4);
					break;
				}
				if(addOffset) {
					if(instruction.offset()) {
						if(abbreviateOffset && !(instruction.offset() & 0xff00)) {
							stream << '+' << to_hex(instruction.offset(), 2);
						} else {
							stream << '+' << to_hex(instruction.offset(), 4);
						}
					}
				}
				stream << ']';
				return stream.str();
			}
		}

		return operand;
	};

	const int operands = num_operands(instruction.operation);
	const bool displacement = has_displacement(instruction.operation);
	operation += " ";
	if(operands > 1) {
		operation += to_string(instruction.destination(), instruction);
		operation += ", ";
	}
	if(operands > 0) {
		operation += to_string(instruction.source(), instruction);
	}
	if(displacement) {
		operation += to_hex(instruction.displacement(), 2);
	}

	return [NSString stringWithUTF8String:operation.c_str()];
}

- (bool)applyDecodingTest:(NSDictionary *)test file:(NSString *)file {
	using Decoder = InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086>;
	Decoder decoder;

	// Build a vector of the instruction bytes; this makes manual step debugging easier.
	NSArray<NSNumber *> *encoding = test[@"bytes"];
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}
	auto log_hex = [&] {
		NSMutableString *hexInstruction = [[NSMutableString alloc] init];
		for(uint8_t byte: data) {
			[hexInstruction appendFormat:@"%02x ", byte];
		}
		NSLog(@"Instruction was %@", hexInstruction);
	};

	const auto decoded = decoder.decode(data.data(), data.size());
	XCTAssert(
		decoded.first == [encoding count],
		"Wrong length of instruction decoded for %@ — decoded %d rather than %lu; file %@",
			test[@"name"],
			decoded.first,
			(unsigned long)[encoding count],
			file
	);

	// The decoder doesn't preserve the original offset length, which makes no functional difference but
	// does affect the way that offsets are printed in the test set.
	NSString *fullOffset = [self toString:decoded.second abbreviateOffset:NO];
	NSString *shortOffset = [self toString:decoded.second abbreviateOffset:YES];
	const bool isEqual = [fullOffset isEqualToString:test[@"name"]] || [shortOffset isEqualToString:test[@"name"]];
	XCTAssert(isEqual, "%@ matches neither %@ nor %@, within %@", test[@"name"], fullOffset, shortOffset, file);

	// Repetition, to allow for easy breakpoint placement.
	if(!isEqual) {
		log_hex();

		// Repeat operand conversions, for debugging.
		Decoder decoder;
		const auto instruction = decoder.decode(data.data(), data.size());
		const InstructionSet::x86::Source sources[] = {
			instruction.second.source().source<false>(),
			instruction.second.destination().source<false>(),
		};
		(void)sources;

//		const auto destination = instruction.second.destination();
//		to_string(destination, instruction.second);
//		const auto source = instruction.second.source();
//		to_string(source, instruction.second);
		return false;
	}

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
			if(![self applyDecodingTest:test file:file]) {
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
