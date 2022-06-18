//
//  65816ComparativeTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 18/06/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#include "65816.hpp"

#import <XCTest/XCTest.h>
#include <array>
#include <vector>
#include <unordered_map>

namespace {

struct BusHandler: public CPU::MOS6502Esque::BusHandler<uint32_t>  {
	// Use a map to store RAM contents, in order to preserve initialised state.
	std::unordered_map<uint32_t, uint8_t> ram;

	Cycles perform_bus_operation(CPU::MOS6502Esque::BusOperation operation, uint32_t address, uint8_t *value) {
		// Record the basics of the operation.
		auto &cycle = cycles.emplace_back();
		cycle.address = address;
		cycle.operation = operation;
		cycle.value = 0xff;

		// Perform the operation, and fill in the cycle's value.
		using BusOperation = CPU::MOS6502Esque::BusOperation;
		auto ram_value = ram.find(address);
		switch(operation) {
			case BusOperation::ReadOpcode:
				++opcodes_fetched_;
			case BusOperation::Read:
			case BusOperation::ReadProgram:
			case BusOperation::ReadVector:
				if(ram_value != ram.end()) {
					cycle.value = *value = ram_value->second;
				} else {
					cycle.value = *value = uint8_t(rand() >> 8);
					ram[address] = cycle.value;
				}
			break;

			case BusOperation::Write:
				cycle.value = ram[address] = *value;
			break;

			default: break;
		}

		// Don't occupy any bonus time.
		return Cycles(1);
	}

	int opcodes_fetched_ = 0;

	struct Cycle {
		CPU::MOS6502Esque::BusOperation operation;
		uint32_t address;
		uint8_t value;
	};
	std::vector<Cycle> cycles;
};

}

// MARK: - New test generator.

@interface TestGenerator : NSObject
@end

@implementation TestGenerator

- (void)generate {
	BusHandler handler;
	CPU::WDC65816::Processor<BusHandler, false> processor(handler);

	for(int operation = 0; operation < 512; operation++) {
		const bool is_emulated = operation & 256;
		const uint8_t opcode = operation & 255;

		// TODO: set up for opcode and emulation mode.

		// TODO: run for a bit longer than this, of course.
		processor.run_for(Cycles(1));
	}

	printf("");
}

@end

// MARK: - Existing test evaluator.

@interface WDC65816ComparativeTests : XCTestCase
@end

@implementation WDC65816ComparativeTests

// A generator for tests; not intended to be a permanent fixture.
- (void)testGenerate {
	[[[TestGenerator alloc] init] generate];
}

@end
