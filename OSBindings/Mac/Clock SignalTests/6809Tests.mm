//
//  68000BCDTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 29/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "Processors/6809/6809.hpp"

#include <unordered_map>

namespace {

// This is a proto-single-step-tests file; I don't yet know the full veracity of its
// source and it doesn't include bus activity.
NSString *const testFile = @"/Users/thomasharte/Scratch/6809tests.json";

struct M6809Capture {
	std::unordered_map<uint16_t, uint8_t> ram;

	template <
		CPU::M6809::BusPhase bus_phase,
		CPU::M6809::ReadWrite read_write,
		CPU::M6809::BusState bus_state,
		typename AddressT
	>
	Cycles perform(
		const AddressT address,
		CPU::M6809::data_t<read_write> value
	) {
		if constexpr (CPU::M6809::is_read(read_write)) {
			const auto entry = ram.find(address);
			if(entry == ram.end()) {
				throw -1;
			}
			value = entry->second;
		} else {
			ram[address] = value;
		}

		return Cycles(0);
	}
};

struct M6809Traits {
	static constexpr bool uses_mrdy = false;
	static constexpr auto pause_precision = CPU::M6809::PausePrecision::BetweenInstructions;
	using BusHandlerT = M6809Capture;
};

}

@interface M6809Tests : XCTestCase
@end

@implementation M6809Tests

- (void)testCase:(NSDictionary *)test {
	M6809Capture capturer;
	CPU::M6809::Processor<M6809Traits> m6809_(capturer);

	NSDictionary *const initial = test[@"initial"];
	m6809_.registers().cc = [initial[@"CC"] intValue];
	m6809_.registers().d.full = [initial[@"D"] intValue];
	m6809_.registers().dp = [initial[@"DP"] intValue];
	m6809_.registers().pc.full = [initial[@"PC"] intValue];
	m6809_.registers().s = [initial[@"S"] intValue];
	m6809_.registers().u = [initial[@"U"] intValue];
	m6809_.registers().x = [initial[@"X"] intValue];
	m6809_.registers().y = [initial[@"Y"] intValue];

	for(NSArray *entry in initial[@"ram"]) {
		capturer.ram[[entry[0] intValue]] = [entry[1] intValue];
	}

	// Don't test illegal opcodes for now.
	const auto opcode = capturer.ram[m6809_.registers().pc.full];
	InstructionSet::M6809::OperationReturner catcher;
	InstructionSet::M6809::OperationMapper<InstructionSet::M6809::Page::Page0> mapper;
	const auto decoded = Reflection::dispatch(mapper, opcode, catcher);
	if(decoded.mode == InstructionSet::M6809::AddressingMode::Illegal) {
		return;
	}

	// Extended pages haven't been handled entirely properly in the underlying JSON.
	if(opcode == 0x10 || opcode == 0x11) {
		return;
	}

	// These are as yet unimplemented.
	switch(decoded.operation) {
		default: break;

		using enum InstructionSet::M6809::Operation;
		case CWAI:
		case SYNC: return;
	}

	if(decoded.operation != InstructionSet::M6809::Operation::DAA) {
		return;
	}

	m6809_.set<CPU::M6809::Line::PowerOnReset>(false);
	m6809_.run_for(1);

	NSDictionary *const end = test[@"final"];
	NSString *identifier = test[@"name"];
	XCTAssertEqual(uint8_t(m6809_.registers().cc), [end[@"CC"] intValue], @"%@", identifier);
	XCTAssertEqual(m6809_.registers().d.full, [end[@"D"] intValue], @"%@", identifier);
	XCTAssertEqual(m6809_.registers().dp, [end[@"DP"] intValue], @"%@", identifier);
	XCTAssertEqual(m6809_.registers().pc.full, [end[@"PC"] intValue], @"%@", identifier);
	XCTAssertEqual(m6809_.registers().s, [end[@"S"] intValue], @"%@", identifier);
	XCTAssertEqual(m6809_.registers().u, [end[@"U"] intValue], @"%@", identifier);
	XCTAssertEqual(m6809_.registers().x, [end[@"X"] intValue], @"%@", identifier);
	XCTAssertEqual(m6809_.registers().y, [end[@"Y"] intValue], @"%@", identifier);

	// TODO: verify RAM contents.
}

- (void)testCaptures {
	NSData *data = [NSData dataWithContentsOfFile:testFile];
	NSArray *tests = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];

	for(NSDictionary *test in tests) {
		[self testCase:test];
	}
}

@end
