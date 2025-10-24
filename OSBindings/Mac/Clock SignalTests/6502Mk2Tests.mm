//
//  6502Mk2Tests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 21/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "6502Mk2.hpp"

#include <vector>

// MARK: - Test paths

// The tests themselves are not duplicated in this repository; provide their real paths here.
constexpr char TestSuiteHome[] = "/Users/thomasharte/Downloads/65x02-main/";

// MARK: - BusHandler

namespace {

struct TestComplete {};

struct BusHandler {
	uint8_t memory[65536];
	int opcode_reads;

	struct Access {
		bool read;
		uint16_t address;
		uint8_t value;
	};
	std::vector<Access> accesses;

	template <CPU::MOS6502Mk2::BusOperation operation, typename AddressT>
	Cycles perform(const AddressT address, CPU::MOS6502Mk2::data_t<operation> data) {
		// Check for end of tests.
		opcode_reads += operation == CPU::MOS6502Mk2::BusOperation::ReadOpcode;
		if(opcode_reads == 2) {
			throw TestComplete{};
		}

		// Perform and record access.
		if constexpr (is_read(operation)) {
			data = memory[address];
			accesses.emplace_back(true, address, data);
		} else {
			memory[address] = data;
			accesses.emplace_back(false, address, data);
		}
		return Cycles(1);
	}
};

struct Traits {
	static constexpr auto uses_ready_line = false;
	static constexpr auto pause_precision = CPU::MOS6502Mk2::PausePrecision::AnyCycle;
	using BusHandlerT = BusHandler;
};

CPU::MOS6502Mk2::Registers registersFrom(NSDictionary *dictionary) {
	CPU::MOS6502Mk2::Registers result;
	result.a = [dictionary[@"a"] intValue];
	result.x = [dictionary[@"x"] intValue];
	result.y = [dictionary[@"y"] intValue];
	result.s = [dictionary[@"s"] intValue];
	result.pc.full = [dictionary[@"pc"] intValue];
	result.flags = CPU::MOS6502Mk2::Flags([dictionary[@"p"] intValue]);
	return result;
}

template <CPU::MOS6502Mk2::Model model>
void testExecution(NSDictionary *test, BusHandler &handler) {
	CPU::MOS6502Mk2::Processor<model, Traits> processor(handler);

	NSDictionary *initial = test[@"initial"];
	const auto initial_registers = registersFrom(initial);
	processor.set_registers(initial_registers);
	for(NSArray *value in initial[@"ram"]) {
		handler.memory[[value[0] intValue]] = [value[1] intValue];
	}

	processor.template set<CPU::MOS6502Mk2::Line::PowerOn>(false);
	handler.opcode_reads = 0;
	handler.accesses.clear();

	try {
		processor.run_for(Cycles(11));	// To catch the entirety of a JAM as in the JSON.
	} catch (TestComplete) {}

	NSDictionary *final = test[@"final"];
	const auto final_registers = registersFrom(final);
	XCTAssertEqual(final_registers.a, processor.registers().a);
	XCTAssertEqual(final_registers.x, processor.registers().x);
	XCTAssertEqual(final_registers.y, processor.registers().y);
	XCTAssertEqual(final_registers.s, processor.registers().s);
	XCTAssert(final_registers.pc == processor.registers().pc);
	XCTAssert(final_registers.flags <=> processor.registers().flags == std::strong_ordering::equal);

	auto found_cycle = handler.accesses.begin();
	for(NSArray *cycle in test[@"cycles"]) {
		XCTAssertNotEqual(found_cycle, handler.accesses.end());

		XCTAssertEqual(found_cycle->address, [cycle[0] intValue]);
		XCTAssertEqual(found_cycle->value, [cycle[1] intValue]);

		NSString *type = cycle[2];
		XCTAssert([type isEqualToString:@"read"] || [type isEqualToString:@"write"]);
		XCTAssertEqual([type isEqualToString:@"read"], found_cycle->read);

		++found_cycle;
	}
	XCTAssertEqual(found_cycle, handler.accesses.end());
}

void testExecution(CPU::MOS6502Mk2::Model model, NSDictionary *test, BusHandler &handler) {
	switch(model) {
		default: __builtin_unreachable();
		case CPU::MOS6502Mk2::Model::NES6502:	testExecution<CPU::MOS6502Mk2::Model::NES6502>(test, handler);	break;
		case CPU::MOS6502Mk2::Model::M6502:	testExecution<CPU::MOS6502Mk2::Model::M6502>(test, handler);	break;
//		case CPU::MOS6502Mk2::Model::Synertek65C02:	testExecution<CPU::MOS6502Mk2::Model::Synertek65C02>(test, handler);	break;
//		case CPU::MOS6502Mk2::Model::Rockwell65C02:	testExecution<CPU::MOS6502Mk2::Model::Rockwell65C02>(test, handler);	break;
//		case CPU::MOS6502Mk2::Model::WDC65C02:	testExecution<CPU::MOS6502Mk2::Model::WDC65C02>(test, handler);	break;
	}
}

}

// MARK: - XCTestCase

@interface m6502Mk2Tests : XCTestCase
@end

@implementation m6502Mk2Tests {
	BusHandler handler;
}

- (void)testFile:(NSString *)file model:(CPU::MOS6502Mk2::Model)model {
	NSLog(@"Starting %@", file);
	NSArray *tests =
		[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:file] options:0 error:nil];
	for(NSDictionary *test in tests) {
		testExecution(model, test, handler);
	}
}

- (void)testPath:(NSString *)path model:(CPU::MOS6502Mk2::Model)model {
	NSLog(@"Into %@", path);
	NSArray<NSString *> *const files =
		[[[NSFileManager defaultManager]
			contentsOfDirectoryAtPath:path
			error:nil
		] sortedArrayUsingSelector:@selector(compare:)];
	for(NSString *file in files) {
		[self testFile:[path stringByAppendingPathComponent:file] model:model];
	}
}

- (void)testAll {
	NSString *const path = [NSString stringWithUTF8String:TestSuiteHome];

	[self testPath:[path stringByAppendingPathComponent:@"nes6502/v1"] model:CPU::MOS6502Mk2::Model::NES6502];
	[self testPath:[path stringByAppendingPathComponent:@"6502/v1"] model:CPU::MOS6502Mk2::Model::M6502];
}

@end
