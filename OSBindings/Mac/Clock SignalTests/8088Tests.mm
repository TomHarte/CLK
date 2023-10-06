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
#include "../../../InstructionSets/x86/Perform.hpp"
#include "../../../Numeric/RegisterSizes.hpp"

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
	NSSet *allowList = [NSSet setWithArray:@[
	]];

	NSSet *ignoreList = nil;

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *) {
		if(allowList.count && ![allowList containsObject:[evaluatedObject lastPathComponent]]) {
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

	return [fullPaths sortedArrayUsingSelector:@selector(compare:)];
}

- (NSArray<NSDictionary *> *)testsInFile:(NSString *)file {
	NSData *data = [NSData dataWithContentsOfGZippedFile:file];
	return [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
}

- (NSString *)toString:(const InstructionSet::x86::Instruction<false> &)instruction offsetLength:(int)offsetLength immediateLength:(int)immediateLength {
	const auto operation = to_string(instruction, InstructionSet::x86::Model::i8086, offsetLength, immediateLength);
	return [[NSString stringWithUTF8String:operation.c_str()] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}

- (std::vector<uint8_t>)bytes:(NSArray<NSNumber *> *)encoding {
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}
	return data;
}

- (bool)applyDecodingTest:(NSDictionary *)test file:(NSString *)file assert:(BOOL)assert {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;

	// Build a vector of the instruction bytes; this makes manual step debugging easier.
	const auto data = [self bytes:test[@"bytes"]];
	auto hex_instruction = [&]() -> NSString * {
		NSMutableString *hexInstruction = [[NSMutableString alloc] init];
		for(uint8_t byte: data) {
			[hexInstruction appendFormat:@"%02x ", byte];
		}
		return hexInstruction;
	};

	const auto decoded = decoder.decode(data.data(), data.size());
	const bool sizeMatched = decoded.first == data.size();
	if(assert) {
		XCTAssert(
			sizeMatched,
			"Wrong length of instruction decoded for %@ — decoded %d rather than %lu from %@; file %@",
				test[@"name"],
				decoded.first,
				(unsigned long)data.size(),
				hex_instruction(),
				file
		);
	}
	if(!sizeMatched) {
		return false;
	}

	// The decoder doesn't preserve the original offset length, which makes no functional difference but
	// does affect the way that offsets are printed in the test set.
	NSSet<NSString *> *decodings = [NSSet setWithObjects:
		[self toString:decoded.second offsetLength:4 immediateLength:4],
		[self toString:decoded.second offsetLength:2 immediateLength:4],
		[self toString:decoded.second offsetLength:0 immediateLength:4],
		[self toString:decoded.second offsetLength:4 immediateLength:2],
		[self toString:decoded.second offsetLength:2 immediateLength:2],
		[self toString:decoded.second offsetLength:0 immediateLength:2],
		nil];

	auto compare_decoding = [&](NSString *name) -> bool {
		return [decodings containsObject:name];
	};

	bool isEqual = compare_decoding(test[@"name"]);

	// Attempt clerical reconciliation:
	//
	// TEMPORARY HACK: the test set incorrectly states 'bp+si' whenever it means 'bp+di'.
	// Though it also uses 'bp+si' correctly when it means 'bp+si'. Until fixed, take
	// a pass on potential issues there.
	//
	// SEPARATELY: The test suite retains a distinction between SHL and SAL, which the decoder doesn't. So consider that
	// a potential point of difference.
	//
	// Also, the decoder treats INT3 and INT 3 as the same thing. So allow for a meshing of those.
	int adjustment = 7;
	while(!isEqual && adjustment) {
		NSString *alteredName = [test[@"name"] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

		if(adjustment & 4) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"bp+si" withString:@"bp+di"];
		}
		if(adjustment & 2) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"shl" withString:@"sal"];
		}
		if(adjustment & 1) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"int3" withString:@"int 03h"];
		}

		isEqual = compare_decoding(alteredName);
		--adjustment;
	}

	if(assert) {
		XCTAssert(
			isEqual,
			"%@ doesn't match %@ or similar, was %@ within %@",
				test[@"name"],
				[decodings anyObject],
				hex_instruction(),
				file
		);
	}

	return isEqual;
}

- (bool)applyExecutionTest:(NSDictionary *)test file:(NSString *)file assert:(BOOL)assert {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;
	const auto data = [self bytes:test[@"bytes"]];
	const auto decoded = decoder.decode(data.data(), data.size());

	struct Registers {
		CPU::RegisterPair16 ax_;
		uint8_t &al()	{	return ax_.halves.low;	}
		uint8_t &ah()	{	return ax_.halves.high;	}
		uint16_t &ax()	{	return ax_.full;		}

		CPU::RegisterPair16 &axp()	{	return ax_;	}

		CPU::RegisterPair16 cx_;
		uint8_t &cl()	{	return cx_.halves.low;	}
		uint8_t &ch()	{	return cx_.halves.high;	}
		uint16_t &cx()	{	return cx_.full;		}

		CPU::RegisterPair16 dx_;
		uint8_t &dl()	{	return dx_.halves.low;	}
		uint8_t &dh()	{	return dx_.halves.high;	}
		uint16_t &dx()	{	return dx_.full;		}

		CPU::RegisterPair16 bx_;
		uint8_t &bl()	{	return bx_.halves.low;	}
		uint8_t &bh()	{	return bx_.halves.high;	}
		uint16_t &bx()	{	return bx_.full;		}

		uint16_t sp_;
		uint16_t &sp()	{	return sp_;				}

		uint16_t bp_;
		uint16_t &bp()	{	return bp_;				}

		uint16_t si_;
		uint16_t &si()	{	return si_;				}

		uint16_t di_;
		uint16_t &di()	{	return di_;				}

		uint16_t es_, cs_, ds_, ss_;
		uint16_t ip_;
	};
	struct Memory {
		std::vector<uint8_t> memory;
		const Registers &registers_;

		Memory(Registers &registers) : registers_(registers) {
			memory.resize(1024*1024);
		}

		template <typename IntT> IntT &access([[maybe_unused]] InstructionSet::x86::Source segment, uint16_t address) {
			uint32_t physical_address;
			using Source = InstructionSet::x86::Source;
			switch(segment) {
				default:			physical_address = registers_.ds_;	break;
				case Source::ES:	physical_address = registers_.es_;	break;
				case Source::CS:	physical_address = registers_.cs_;	break;
				case Source::DS:	physical_address = registers_.ds_;	break;
			}
			physical_address = ((physical_address << 4) + address) & 0xf'ffff;
			return *reinterpret_cast<IntT *>(&memory[physical_address]);
		}
	};
	struct IO {
	};
	struct FlowController {
	};

	InstructionSet::x86::Status status;
	FlowController flow_controller;
	Registers registers;
	Memory memory(registers);
	IO io;

	// Apply initial state.
	NSDictionary *const initial = test[@"initial"];
	for(NSArray<NSNumber *> *ram in initial[@"ram"]) {
		memory.memory[[ram[0] intValue]] = [ram[1] intValue];
	}
	NSDictionary *const initial_registers = initial[@"regs"];
	registers.ax_.full = [initial_registers[@"ax"] intValue];
	registers.bx_.full = [initial_registers[@"bx"] intValue];
	registers.cx_.full = [initial_registers[@"cx"] intValue];
	registers.dx_.full = [initial_registers[@"dx"] intValue];

	registers.bp_ = [initial_registers[@"bp"] intValue];
	registers.cs_ = [initial_registers[@"cs"] intValue];
	registers.di_ = [initial_registers[@"di"] intValue];
	registers.ds_ = [initial_registers[@"ds"] intValue];
	registers.es_ = [initial_registers[@"es"] intValue];
	registers.si_ = [initial_registers[@"si"] intValue];
	registers.sp_ = [initial_registers[@"sp"] intValue];
	registers.ss_ = [initial_registers[@"ss"] intValue];
	registers.ip_ = [initial_registers[@"ip"] intValue];

	status.set([initial_registers[@"flags"] intValue]);

	InstructionSet::x86::perform<InstructionSet::x86::Model::i8086>(
		decoded.second,
		status,
		flow_controller,
		registers,
		memory,
		io
	);

	return false;
}

- (void)printFailures:(NSArray<NSString *> *)failures {
	NSLog(
		@"%ld failures out of %ld tests: %@",
		failures.count,
		[self testFiles].count,
		[failures sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]);
}

- (void)testDecoding {
	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];
	for(NSString *file in [self testFiles]) {
		for(NSDictionary *test in [self testsInFile:file]) {
			// A single failure per instruction is fine.
			if(![self applyDecodingTest:test file:file assert:YES]) {
				[failures addObject:file];

				// Attempt a second decoding, to provide a debugger hook.
				[self applyDecodingTest:test file:file assert:NO];
				break;
			}
		}
	}

	[self printFailures:failures];
}

- (void)testExecution {
	NSMutableArray<NSString *> *failures = [[NSMutableArray alloc] init];
	for(NSString *file in [self testFiles]) {
		for(NSDictionary *test in [self testsInFile:file]) {
			// A single failure per instruction is fine.
			if(![self applyExecutionTest:test file:file assert:YES]) {
				[failures addObject:file];

				// Attempt a second decoding, to provide a debugger hook.
				[self applyExecutionTest:test file:file assert:NO];
				break;
			}
		}
	}

	[self printFailures:failures];
}

@end
