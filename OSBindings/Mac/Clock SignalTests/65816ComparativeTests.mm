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

struct StopException {};

struct BusHandler: public CPU::MOS6502Esque::BusHandler<uint32_t>  {
	// Use a map to store RAM contents, in order to preserve initialised state.
	std::unordered_map<uint32_t, uint8_t> ram;
	std::unordered_map<uint32_t, uint8_t> inventions;

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
				--opcodes_remaining;
				if(!opcodes_remaining) {
					cycles.pop_back();
					throw StopException();
				}
			case BusOperation::Read:
			case BusOperation::ReadProgram:
			case BusOperation::ReadVector:
				if(ram_value != ram.end()) {
					cycle.value = *value = ram_value->second;
				} else {
					cycle.value = *value = uint8_t(rand() >> 8);
					inventions[address] = ram[address] = cycle.value;
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

	template <typename Processor> void setup(Processor &processor, uint8_t opcode) {
		ram.clear();
		inventions.clear();
		cycles.clear();

		using Register = CPU::MOS6502Esque::Register;
		const uint32_t pc =
			processor.get_value_of_register(Register::ProgramCounter) |
			(processor.get_value_of_register(Register::ProgramBank) << 8);
		inventions[pc] = ram[pc] = opcode;
	}

	int opcodes_remaining = 0;

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

	// Never run the official reset procedure.
	processor.set_power_on(false);

	for(int operation = 0; operation < 512; operation++) {
		const bool is_emulated = operation & 256;
		const uint8_t opcode = operation & 255;

		// Ensure processor's next action is an opcode fetch.
		processor.restart_operation_fetch();

		// Randomise processor state.
		using Register = CPU::MOS6502Esque::Register;
		processor.set_value_of_register(Register::EmulationFlag, is_emulated);

		// Establish the opcode.
		handler.setup(processor, opcode);

		// Run to the second opcode fetch.
		handler.opcodes_remaining = 2;
		try {
			processor.run_for(Cycles(100));
		} catch (const StopException &) {}
	}
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
