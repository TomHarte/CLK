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

using Status = InstructionSet::x86::Status;
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

	bool operator ==(const Registers &rhs) const {
		return
			ax_.full == rhs.ax_.full &&
			cx_.full == rhs.cx_.full &&
			dx_.full == rhs.dx_.full &&
			bx_.full == rhs.bx_.full &&
			sp_ == rhs.sp_ &&
			bp_ == rhs.bp_ &&
			si_ == rhs.si_ &&
			di_ == rhs.di_ &&
			es_ == rhs.es_ &&
			cs_ == rhs.cs_ &&
			ds_ == rhs.ds_ &&
			si_ == rhs.si_ &&
			ip_ == rhs.ip_;
	}
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
			case Source::SS:	physical_address = registers_.ss_;	break;
		}
		physical_address = ((physical_address << 4) + address) & 0xf'ffff;
		return access<IntT>(physical_address);
	}

	template <typename IntT> IntT &access(uint32_t address) {
		return *reinterpret_cast<IntT *>(&memory[address]);
	}
};
struct IO {
};
class FlowController {
	public:
		FlowController(Memory &memory, Registers &registers, Status &status) :
			memory_(memory), registers_(registers), status_(status) {}

		void interrupt(int index) {
			const uint16_t address = static_cast<uint16_t>(index) << 2;
			const uint16_t new_ip = memory_.access<uint16_t>(address);
			const uint16_t new_cs = memory_.access<uint16_t>(address + 2);

			push(status_.get());

			// TODO: set I and TF
//			status_.

			// Push CS and IP.
			push(registers_.cs_);
			push(registers_.ip_);

			registers_.cs_ = new_cs;
			registers_.ip_ = new_ip;
		}

	private:
		Memory &memory_;
		Registers &registers_;
		Status status_;

		void push(uint16_t value) {
			--registers_.sp_;
			memory_.access<uint8_t>(InstructionSet::x86::Source::SS, registers_.sp_) = value >> 8;
			--registers_.sp_;
			memory_.access<uint8_t>(InstructionSet::x86::Source::SS, registers_.sp_) = value & 0xff;
		}
};

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];
	NSSet *allowList = [NSSet setWithArray:@[
		@"D4.json.gz"
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

- (void)populate:(Registers &)registers status:(InstructionSet::x86::Status &)status value:(NSDictionary *)value {
	registers.ax_.full = [value[@"ax"] intValue];
	registers.bx_.full = [value[@"bx"] intValue];
	registers.cx_.full = [value[@"cx"] intValue];
	registers.dx_.full = [value[@"dx"] intValue];

	registers.bp_ = [value[@"bp"] intValue];
	registers.cs_ = [value[@"cs"] intValue];
	registers.di_ = [value[@"di"] intValue];
	registers.ds_ = [value[@"ds"] intValue];
	registers.es_ = [value[@"es"] intValue];
	registers.si_ = [value[@"si"] intValue];
	registers.sp_ = [value[@"sp"] intValue];
	registers.ss_ = [value[@"ss"] intValue];
	registers.ip_ = [value[@"ip"] intValue];

	status.set([value[@"flags"] intValue]);
}

- (bool)applyExecutionTest:(NSDictionary *)test file:(NSString *)file assert:(BOOL)assert {
	InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086> decoder;
	const auto data = [self bytes:test[@"bytes"]];
	const auto decoded = decoder.decode(data.data(), data.size());

	InstructionSet::x86::Status status;
	Registers registers;
	Memory memory(registers);
	FlowController flow_controller(memory, registers, status);
	IO io;

	// Apply initial state.
	NSDictionary *const initial_state = test[@"initial"];
	for(NSArray<NSNumber *> *ram in initial_state[@"ram"]) {
		memory.memory[[ram[0] intValue]] = [ram[1] intValue];
	}
	[self populate:registers status:status value:initial_state[@"regs"]];

	// Execute instruction.
	registers.ip_ += decoded.first;
	InstructionSet::x86::perform<InstructionSet::x86::Model::i8086>(
		decoded.second,
		status,
		flow_controller,
		registers,
		memory,
		io
	);

	// Compare final state.
	NSDictionary *const final_state = test[@"final"];
	Registers intended_registers;
	InstructionSet::x86::Status intended_status;

	bool ramEqual = true;
	for(NSArray<NSNumber *> *ram in final_state[@"ram"]) {
		ramEqual &= memory.memory[[ram[0] intValue]] == [ram[1] intValue];
	}

	[self populate:intended_registers status:intended_status value:final_state[@"regs"]];
	const bool registersEqual = intended_registers == registers;
	const bool statusEqual = intended_status == status;

	if(assert) {
		XCTAssert(
			statusEqual,
			"Status doesn't match — differs in %02x after %@; executing %@",
				intended_status.get() ^ status.get(),
				test[@"name"],
				[self toString:decoded.second offsetLength:4 immediateLength:4]
		);
		// TODO: should probably say more about the following two.
		XCTAssert(
			registersEqual,
			"Register mismatch after %@; executing %@",
				test[@"name"],
				[self toString:decoded.second offsetLength:4 immediateLength:4]
		);
		XCTAssert(
			ramEqual,
			"Memory contents mismatch after %@; executing %@",
				test[@"name"],
				[self toString:decoded.second offsetLength:4 immediateLength:4]
		);
	}

	return statusEqual && registersEqual && ramEqual;
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
