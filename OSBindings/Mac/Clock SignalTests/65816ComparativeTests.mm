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
#include <optional>
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
		cycle.extended_bus = processor.get_extended_bus_output();

		// Perform the operation, and fill in the cycle's value.
		using BusOperation = CPU::MOS6502Esque::BusOperation;
		auto ram_value = ram.find(address);
		switch(operation) {
			case BusOperation::ReadOpcode:
				if(initial_pc && (*initial_pc != address || !allow_pc_repetition)) {
					cycles.pop_back();
					pc_overshoot = -1;
					throw StopException();
				}
				initial_pc = address;
			case BusOperation::Read:
			case BusOperation::ReadProgram:
			case BusOperation::ReadVector:
				if(ram_value != ram.end()) {
					cycle.value = *value = ram_value->second;
				} else {
					cycle.value = *value = uint8_t(rand() >> 8);
					inventions[address] = ram[address] = *cycle.value;
				}
			break;

			case BusOperation::Write:
				cycle.value = ram[address] = *value;
			break;

			case BusOperation::Ready:
			case BusOperation::None:
				throw StopException();
			break;

			case BusOperation::InternalOperationRead:
			case BusOperation::InternalOperationWrite:
			break;

			default: assert(false);
		}

		// Don't occupy any bonus time.
		return Cycles(1);
	}

	void setup(uint8_t opcode) {
		ram.clear();
		inventions.clear();
		cycles.clear();
		pc_overshoot = 0;
		initial_pc = std::nullopt;

		// For MVP or MVN, keep tracking fetches via the same location.
		// For other instructions, don't. That's to avoid endless loops
		// for flow control that happens to jump back to where it began.
		allow_pc_repetition = opcode == 0x54 || opcode == 0x44;

		using Register = CPU::MOS6502Esque::Register;
		const uint32_t pc =
			processor.get_value_of_register(Register::ProgramCounter) |
			(processor.get_value_of_register(Register::ProgramBank) << 16);
		inventions[pc] = ram[pc] = opcode;
	}

	int pc_overshoot = 0;
	std::optional<uint32_t> initial_pc;
	bool allow_pc_repetition = false;

	struct Cycle {
		CPU::MOS6502Esque::BusOperation operation;
		uint32_t address;
		std::optional<uint8_t> value;
		int extended_bus;
	};
	std::vector<Cycle> cycles;

	CPU::WDC65816::Processor<BusHandler, false> processor;

	BusHandler() : processor(*this) {
		// Never run the official reset procedure.
		processor.set_power_on(false);
	}
};

template <typename Processor> void print_registers(FILE *file, const Processor &processor, int pc_offset) {
	using Register = CPU::MOS6502Esque::Register;
	fprintf(file, "\"pc\": %d, ", (processor.get_value_of_register(Register::ProgramCounter) + pc_offset) & 65535);
	fprintf(file, "\"s\": %d, ", processor.get_value_of_register(Register::StackPointer));
	fprintf(file, "\"p\": %d, ", processor.get_value_of_register(Register::Flags));
	fprintf(file, "\"a\": %d, ", processor.get_value_of_register(Register::A));
	fprintf(file, "\"x\": %d, ", processor.get_value_of_register(Register::X));
	fprintf(file, "\"y\": %d, ", processor.get_value_of_register(Register::Y));
	fprintf(file, "\"dbr\": %d, ", processor.get_value_of_register(Register::DataBank));
	fprintf(file, "\"d\": %d, ", processor.get_value_of_register(Register::Direct));
	fprintf(file, "\"pbr\": %d, ", processor.get_value_of_register(Register::ProgramBank));
	fprintf(file, "\"e\": %d, ", processor.get_value_of_register(Register::EmulationFlag));
}

void print_ram(FILE *file, const std::unordered_map<uint32_t, uint8_t> &data) {
	fprintf(file, "\"ram\": [");
	bool is_first = true;
	for(const auto &pair: data) {
		if(!is_first) fprintf(file, ", ");
		is_first = false;
		fprintf(file, "[%d, %d]", pair.first, pair.second);
	}
	fprintf(file, "]");
}

}

// MARK: - New test generator.

@interface TestGenerator : NSObject
@end

@implementation TestGenerator

- (void)generate {
	BusHandler handler;

	// Make tests repeatable, at least for any given instance of
	// the runtime.
	srand(65816);

	NSString *const tempDir = NSTemporaryDirectory();
	NSLog(@"Outputting to %@", tempDir);

	for(int operation = 0; operation < 512; operation++) {
		const bool is_emulated = operation & 256;
		const uint8_t opcode = operation & 255;

		NSString *const targetName = [NSString stringWithFormat:@"%@%02x.%c.json", tempDir, opcode, is_emulated ? 'e' : 'n'];
		FILE *const target = fopen(targetName.UTF8String, "wt");

		bool is_first_test = true;
		fprintf(target, "[");
		for(int test = 0; test < 10'000; test++) {
			if(!is_first_test) fprintf(target, ",\n");
			is_first_test = false;

			// Ensure processor's next action is an opcode fetch.
			handler.processor.restart_operation_fetch();

			// Randomise most of the processor state...
			using Register = CPU::MOS6502Esque::Register;
			handler.processor.set_value_of_register(Register::A, rand() >> 8);
			handler.processor.set_value_of_register(Register::Flags, rand() >> 8);
			handler.processor.set_value_of_register(Register::X, rand() >> 8);
			handler.processor.set_value_of_register(Register::Y, rand() >> 8);
			handler.processor.set_value_of_register(Register::ProgramCounter, rand() >> 8);
			handler.processor.set_value_of_register(Register::StackPointer, rand() >> 8);
			handler.processor.set_value_of_register(Register::DataBank, rand() >> 8);
			handler.processor.set_value_of_register(Register::ProgramBank, rand() >> 8);
			handler.processor.set_value_of_register(Register::Direct, rand() >> 8);

			// ... except for emulation mode, which is a given.
			// And is set last to ensure proper internal state is applied.
			handler.processor.set_value_of_register(Register::EmulationFlag, is_emulated);

			// Establish the opcode.
			handler.setup(opcode);

			// Dump initial state.
			fprintf(target, "{ \"name\": \"%02x %c %d\", ", opcode, is_emulated ? 'e' : 'n', test + 1);
			fprintf(target, "\"initial\": {");
			print_registers(target, handler.processor, 0);

			// Run to the second opcode fetch.
			try {
				handler.processor.run_for(Cycles(100));
			} catch (const StopException &) {}

			// Dump all inventions as initial memory state.
			print_ram(target, handler.inventions);

			// Dump final state.
			fprintf(target, "}, \"final\": {");
			print_registers(target, handler.processor, handler.pc_overshoot);
			print_ram(target, handler.ram);
			fprintf(target, "}, ");

			// Append cycles.
			fprintf(target, "\"cycles\": [");

			bool is_first_cycle = true;
			for(const auto &cycle: handler.cycles) {
				if(!is_first_cycle) fprintf(target, ",");
				is_first_cycle = false;

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
					case BusOperation::ReadVector:				read = vpb = vda = true;	break;
					case BusOperation::InternalOperationRead:	read = true;				break;

					case BusOperation::Write:					vda = true;					break;
					case BusOperation::InternalOperationWrite:								break;

					case BusOperation::None:
					case BusOperation::Ready:					wait = true;				break;

					default:
						assert(false);
				}

				using ExtendedBusOutput = CPU::WDC65816::ExtendedBusOutput;
				const bool emulation = cycle.extended_bus & ExtendedBusOutput::Emulation;
				const bool memory_size = cycle.extended_bus & ExtendedBusOutput::MemorySize;
				const bool index_size = cycle.extended_bus & ExtendedBusOutput::IndexSize;
				const bool memory_lock = cycle.extended_bus & ExtendedBusOutput::MemoryLock;

				fprintf(target, "[%d, ", cycle.address);
				if(cycle.value) {
					fprintf(target, "%d, ", *cycle.value);
				} else {
					fprintf(target, "null, ");
				}
				fprintf(target, "\"%c%c%c%c%c%c%c%c\"]",
					vda ? 'd' : '-',
					vpa ? 'p' : '-',
					vpb ? 'v' : '-',
					wait ? '-' : (read ? 'r' : 'w'),
					wait ? '-' : (emulation ? 'e' : '-'),
					wait ? '-' : (memory_size ? 'm' : '-'),
					wait ? '-' : (index_size ? 'x' : '-'),
					wait ? '-' : (memory_lock ? 'l' : '-')
				);
			}

			// Terminate object.
			fprintf(target, "]}");
		}

		fprintf(target, "]");
		fclose(target);
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
