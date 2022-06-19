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
			(processor.get_value_of_register(Register::ProgramBank) << 16);
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

template <typename Processor> void print_registers(const Processor &processor, int pc_offset) {
	using Register = CPU::MOS6502Esque::Register;
	printf("\"pc\": %d, ", (processor.get_value_of_register(Register::ProgramCounter) + pc_offset) & 65535);
	printf("\"s\": %d, ", processor.get_value_of_register(Register::StackPointer));
	printf("\"p\": %d, ", processor.get_value_of_register(Register::Flags));
	printf("\"a\": %d, ", processor.get_value_of_register(Register::A));
	printf("\"x\": %d, ", processor.get_value_of_register(Register::X));
	printf("\"y\": %d, ", processor.get_value_of_register(Register::Y));
	printf("\"dbr\": %d, ", processor.get_value_of_register(Register::DataBank));
	printf("\"d\": %d, ", processor.get_value_of_register(Register::Direct));
	printf("\"pbr\": %d, ", processor.get_value_of_register(Register::ProgramBank));
	printf("\"e\": %d, ", processor.get_value_of_register(Register::EmulationFlag));
}

void print_ram(const std::unordered_map<uint32_t, uint8_t> &data) {
	printf("\"ram\": [");
	bool is_first = true;
	for(const auto &pair: data) {
		if(!is_first) printf(", ");
		is_first = false;
		printf("[%d, %d]", pair.first, pair.second);
	}
	printf("]");
}

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

	// Make tests repeatable, at least for any given instance of
	// the runtime.
	srand(65816);

	for(int operation = 0; operation < 512; operation++) {
		const bool is_emulated = operation & 256;
		const uint8_t opcode = operation & 255;

		for(int test = 0; test < 1; test++) {
			// Ensure processor's next action is an opcode fetch.
			processor.restart_operation_fetch();

			// Randomise most of the processor state...
			using Register = CPU::MOS6502Esque::Register;
			processor.set_value_of_register(Register::A, rand() >> 8);
			processor.set_value_of_register(Register::Flags, rand() >> 8);
			processor.set_value_of_register(Register::X, rand() >> 8);
			processor.set_value_of_register(Register::Y, rand() >> 8);
			processor.set_value_of_register(Register::ProgramCounter, rand() >> 8);
			processor.set_value_of_register(Register::StackPointer, rand() >> 8);
			processor.set_value_of_register(Register::DataBank, rand() >> 8);
			processor.set_value_of_register(Register::ProgramBank, rand() >> 8);
			processor.set_value_of_register(Register::Direct, rand() >> 8);

			// ... except for emulation mode, which is a given.
			// And is set last to ensure proper internal state is applied.
			processor.set_value_of_register(Register::EmulationFlag, is_emulated);

			// Establish the opcode.
			handler.setup(processor, opcode);

			// Dump initial state.
			printf("{ \"name\": \"%02x %c %d\", ", opcode, is_emulated ? 'e' : 'n', test + 1);
			printf("\"initial\": {");
			print_registers(processor, 0);

			// Run to the second opcode fetch.
			handler.opcodes_remaining = 2;
			try {
				processor.run_for(Cycles(100));
			} catch (const StopException &) {}

			// Dump all inventions as initial memory state.
			print_ram(handler.inventions);

			// Dump final state.
			printf("}, \"final\": {");
			print_registers(processor, -1);
			print_ram(handler.ram);
			printf("}, ");

			// Append cycles.
			printf("\"cycles\": [");

			bool is_first = true;
			for(const auto &cycle: handler.cycles) {
				if(!is_first) printf(",");
				is_first = false;

				bool vda = false;
				bool vpa = false;
				bool vpb = false;
				bool read = false;
				bool wait = false;
				using BusOperation = CPU::MOS6502Esque::BusOperation;
				switch(cycle.operation) {
					case BusOperation::Read:					read = vda = true;			break;
					case BusOperation::ReadOpcode:				read = vda = vpa = true;	break;
					case BusOperation::ReadProgram:				read = vpa = true;			break;
					case BusOperation::ReadVector:				read = vpb = true;			break;
					case BusOperation::InternalOperationRead:	read = true;				break;

					case BusOperation::Write:					vda = true;					break;
					case BusOperation::InternalOperationWrite:								break;

					case BusOperation::None:
					case BusOperation::Ready:					wait = true;				break;

					default:
						assert(false);
				}

				printf("[%d, %d, %c%c%c%c]",
					cycle.address,
					cycle.value,
					vda ? 'd' : '-',
					vpa ? 'p' : '-',
					vpb ? 'v' : '-',
					wait ? '-' : (read ? 'r' : 'w'));
			}

			// Terminate object.
			printf("]},\n");
		}
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
