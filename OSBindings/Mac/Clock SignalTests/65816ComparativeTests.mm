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
#include <unordered_set>

#include "6502Selector.hpp"

namespace {

const char *SourceTests65816 = "/Users/thomasharte/Projects/65816/v1";

struct StopException {};

template <CPU::MOS6502Esque::Type type>
struct BusHandler: public CPU::MOS6502Esque::BusHandlerT<type> {
	using AddressType = typename CPU::MOS6502Esque::BusHandlerT<type>::AddressType;

	// Use a map to store RAM contents, in order to preserve initialised state.
	std::unordered_map<AddressType, uint8_t> ram;
	std::unordered_map<AddressType, uint8_t> inventions;

	Cycles perform_bus_operation(const CPU::MOS6502Esque::BusOperation operation, const AddressType address, uint8_t *const value) {
		// Check for a JAM; if one is found then record just five more bus cycles, arbitrarily.
		if(jam_count) {
			--jam_count;
			if(!jam_count) {
				throw StopException();
			}
		} else if(processor.is_jammed()) {
			jam_count = 5;
		}

		// Record the basics of the operation.
		auto &cycle = cycles.emplace_back();
		cycle.operation = operation;
		if constexpr (has_extended_bus_output(type)) {
			cycle.extended_bus = processor.get_extended_bus_output();
		}

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
				[[fallthrough]];

			case BusOperation::Read:
			case BusOperation::ReadProgram:
			case BusOperation::ReadVector:
				cycle.address = address;
				if(ram_value != ram.end()) {
					cycle.value = *value = ram_value->second;
				} else {
					cycle.value = *value = uint8_t(rand() >> 8);
					inventions[address] = ram[address] = *cycle.value;
				}
			break;

			case BusOperation::Write:
				cycle.address = address;
				cycle.value = ram[address] = *value;
			break;

			case BusOperation::Ready:
			case BusOperation::None:
				throw StopException();
			break;

			case BusOperation::InternalOperationWrite:
				cycle.value = *value = ram_value->second;
				[[fallthrough]];

			case BusOperation::InternalOperationRead:
				cycle.address = address;
			break;

			default: assert(false);
		}

		// Don't occupy any bonus time.
		return Cycles(1);
	}

	void setup() {
		ram.clear();
		inventions.clear();
		cycles.clear();
		pc_overshoot = 0;
		initial_pc = std::nullopt;

		allow_pc_repetition = false;
	}

	void setup(const uint8_t opcode) {
		setup();

		// For MVP or MVN, keep tracking fetches via the same location.
		// For other instructions, don't. That's to avoid endless loops
		// for flow control that happens to jump back to where it began.
		allow_pc_repetition = opcode == 0x54 || opcode == 0x44;

		using Register = CPU::MOS6502Esque::Register;
		const auto pc =
			AddressType(
				processor.value_of(Register::ProgramCounter) |
				(processor.value_of(Register::ProgramBank) << 16)
			);
		inventions[pc] = ram[pc] = opcode;
	}

	void invent(const uint32_t address, const uint8_t value) {
		inventions[address] = ram[address] = value;
	}

	int pc_overshoot = 0;
	std::optional<AddressType> initial_pc;
	bool allow_pc_repetition = false;
	int jam_count = 0;

	struct Cycle {
		CPU::MOS6502Esque::BusOperation operation;
		std::optional<AddressType> address;
		std::optional<uint8_t> value;
		int extended_bus = 0;
	};
	std::vector<Cycle> cycles;

	CPU::MOS6502Esque::Processor<type, BusHandler<type>, false> processor;

	BusHandler() : processor(*this) {
		// Never run the official reset procedure.
		processor.set_power_on(false);
	}
};

template <bool has_emulation, typename Processor> void print_registers(FILE *file, const Processor &processor, int pc_offset, std::optional<int> s_override) {
	using Register = CPU::MOS6502Esque::Register;
	fprintf(file, "\"pc\": %d, ", (processor.value_of(Register::ProgramCounter) + pc_offset) & 65535);
	fprintf(file, "\"s\": %d, ", s_override.value_or(processor.value_of(Register::StackPointer)));
	fprintf(file, "\"p\": %d, ", processor.value_of(Register::Flags));
	fprintf(file, "\"a\": %d, ", processor.value_of(Register::A));
	fprintf(file, "\"x\": %d, ", processor.value_of(Register::X));
	fprintf(file, "\"y\": %d, ", processor.value_of(Register::Y));
	if constexpr (has_emulation) {
		fprintf(file, "\"dbr\": %d, ", processor.value_of(Register::DataBank));
		fprintf(file, "\"d\": %d, ", processor.value_of(Register::Direct));
		fprintf(file, "\"pbr\": %d, ", processor.value_of(Register::ProgramBank));
		fprintf(file, "\"e\": %d, ", processor.value_of(Register::EmulationFlag));
	}
}

template <typename IntT>
void print_ram(FILE *file, const std::unordered_map<IntT, uint8_t> &data, NSArray *prior) {
	fprintf(file, "\"ram\": [");

	bool is_first = true;
	const auto append = [&](const IntT address, const uint8_t value) {
		if(!is_first) fprintf(file, ", ");
		is_first = false;
		fprintf(file, "[%d, %d]", address, value);
	};

	// Output anything that is also in prior, in the order it's in prior.
	std::unordered_set<IntT> already_output;
	for(NSArray *value in prior) {
		const IntT address = [value[0] intValue];

		const auto new_value = data.find(address);
		if(new_value == data.end()) {
			continue;
		}

		append(address, new_value->second);
		already_output.insert(address);
	}

	// Output all values not in prior.
	for(const auto &pair: data) {
		if(already_output.find(pair.first) != already_output.end()) {
			continue;
		}

		append(pair.first, pair.second);
	}
	fprintf(file, "]");
}

template <CPU::MOS6502Esque::Type type>
struct TestWriter {
	TestWriter() : tempDir(NSTemporaryDirectory()) {
		NSLog(@"Outputting to %@", tempDir);
	}

	void set_operation(const uint8_t opcode, const bool is_emulated) {
		opcode_ = opcode;
		is_emulated_ = is_emulated;
		set_target_name(
			has_emulation ?
				[NSString stringWithFormat:@"%02x.%c.json", opcode, is_emulated ? 'e' : 'n'] :
				[NSString stringWithFormat:@"%02x.json", opcode]
		);
	}

	void set_target_name(NSString *targetName) {
		target = fopen([tempDir stringByAppendingPathComponent:targetName].UTF8String, "wt");

		is_first_test = true;
		fprintf(target, "[");
	}

	void end_operation() {
		fprintf(target, "]");
		fclose(target);
	}

	void begin_test() {
		if(!is_first_test) fprintf(target, ",\n");
		is_first_test = false;

		// Ensure processor's next action is an opcode fetch.
		handler.processor.restart_operation_fetch();
	}

	void write_initial_state(const int test) {
		// Dump initial state.
		if(has_emulation) {
			fprintf(target, "{ \"name\": \"%02x %c %d\", ", opcode_, is_emulated_ ? 'e' : 'n', test + 1);
		} else {
			fprintf(target, "{ \"name\": \"%02x %d\", ", opcode_, test + 1);
		}
		fprintf(target, "\"initial\": {");
		print_registers<has_emulation>(target, handler.processor, 0);
	}

	void write_initial_state(NSString *name, std::optional<int> s_override) {
		fprintf(target, "{ \"name\": \"%s\", ", name.UTF8String);
		fprintf(target, "\"initial\": {");
		print_registers<has_emulation>(target, handler.processor, 0, s_override);
	}

	void write_initial_ram(NSArray *prior = nil) {
		print_ram(target, handler.inventions, prior);
	}

	void end_test(NSArray *prior_ram = nil) {
		fprintf(target, "}, \"final\": {");
		print_registers<has_emulation>(target, handler.processor, handler.pc_overshoot, std::nullopt);
		print_ram(target, handler.ram, prior_ram);
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

			fprintf(target, "[");
			if(cycle.address) {
				fprintf(target, "%d, ", *cycle.address);
			} else {
				fprintf(target, "null, ");
			}
			if(cycle.value) {
				fprintf(target, "%d, ", *cycle.value);
			} else {
				fprintf(target, "null, ");
			}

			if(has_emulation) {
				using ExtendedBusOutput = CPU::WDC65816::ExtendedBusOutput;
				const bool emulation = cycle.extended_bus & ExtendedBusOutput::Emulation;
				const bool memory_size = cycle.extended_bus & ExtendedBusOutput::MemorySize;
				const bool index_size = cycle.extended_bus & ExtendedBusOutput::IndexSize;
				const bool memory_lock = cycle.extended_bus & ExtendedBusOutput::MemoryLock;

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
			} else {
				if(read) {
					fprintf(target, "\"read\"]");
				} else {
					fprintf(target, "\"write\"]");
				}
			}
		}

		// Terminate object.
		fprintf(target, "]}");
	}

	BusHandler<type> handler;
	static constexpr bool has_emulation = has(type, CPU::MOS6502Esque::Register::EmulationFlag);

private:
	NSString *const tempDir;
	FILE *target = NULL;
	bool is_first_test;
	uint8_t opcode_;
	bool is_emulated_;
};


// MARK: - New test generator.

/// Regression tests against @c source outputting a new set of JSON files that differ anywhere
/// the current 65816 differs in result from the regression test.
template <CPU::MOS6502Esque::Type type> void test(const char *source) {
	TestWriter<type> writer;

	NSString *sourcePath = [NSString stringWithUTF8String:source];
	NSArray<NSString *> *const files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:sourcePath error:nil];
	for(NSString *file in files) {
		writer.set_target_name(file);

		NSArray *tests =
			[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[sourcePath stringByAppendingPathComponent:file]] options:0 error:nil];
		for(NSDictionary *test in tests) {
			writer.begin_test();

			NSDictionary *const initial = test[@"initial"];

			using Register = CPU::MOS6502Esque::Register;
			if(writer.has_emulation) {
				writer.handler.processor.set_value_of(Register::DataBank, [initial[@"dbr"] intValue]);
				writer.handler.processor.set_value_of(Register::ProgramBank, [initial[@"pbr"] intValue]);
				writer.handler.processor.set_value_of(Register::Direct, [initial[@"d"] intValue]);
				writer.handler.processor.set_value_of(Register::EmulationFlag, [initial[@"e"] intValue]);
			}

			writer.handler.processor.set_value_of(Register::A, [initial[@"a"] intValue]);
			writer.handler.processor.set_value_of(Register::Flags, [initial[@"p"] intValue]);
			writer.handler.processor.set_value_of(Register::X, [initial[@"x"] intValue]);
			writer.handler.processor.set_value_of(Register::Y, [initial[@"y"] intValue]);
			writer.handler.processor.set_value_of(Register::ProgramCounter, [initial[@"pc"] intValue]);
			writer.handler.processor.set_value_of(Register::StackPointer, [initial[@"s"] intValue]);

			writer.handler.setup();		// TODO: I think I need to set this up slightly more. Otherwise allow_pc_repetition is unknown.
			writer.write_initial_state(test[@"name"], [initial[@"s"] intValue]);

			// Populate 'inventions'. Bit backwards, but here we are.
			NSArray *initialRAM = initial[@"ram"];
			for(NSArray *value in initialRAM) {
				writer.handler.invent([value[0] intValue], [value[1] intValue]);
			}

			// Run to the second opcode fetch.
			try {
				writer.handler.processor.run_for(Cycles(100));
			} catch (const StopException &) {}

			// Dump all inventions as initial memory state. Provide old RAM array to match ordering.
			writer.write_initial_ram(initialRAM);

			// Dump final state.
			writer.end_test(test[@"final"][@"ram"]);
		}

		writer.end_operation();
//		NSLog(@"%@", @([tests count]));
	}
}

/// Generates an entirely new set of 65816 tests.
template <CPU::MOS6502Esque::Type type> void generate() {
	TestWriter<type> writer;

	for(int operation = 0; operation < (writer.has_emulation ? 512 : 256); operation++) {
		// Make tests repeatable, at least for any given instance of
		// the runtime.
		static constexpr auto type_offset = int(CPU::MOS6502Esque::Type::TWDC65816) - int(type);
		srand(65816 + operation + type_offset);

		const bool is_emulated = operation & 256;
		const uint8_t opcode = operation & 255;
		writer.set_operation(opcode, is_emulated);

		for(int test = 0; test < 10'000; test++) {
			writer.begin_test();

			// Randomise most of the processor state...
			using Register = CPU::MOS6502Esque::Register;
			if(writer.has_emulation) {
				writer.handler.processor.set_value_of(Register::DataBank, rand() >> 8);
				writer.handler.processor.set_value_of(Register::ProgramBank, rand() >> 8);
				writer.handler.processor.set_value_of(Register::Direct, rand() >> 8);

				// ... except for emulation mode, which is a given.
				// And is set last to ensure proper internal state is applied.
				writer.handler.processor.set_value_of(Register::EmulationFlag, is_emulated);
			}

			writer.handler.processor.set_value_of(Register::A, rand() >> 8);
			writer.handler.processor.set_value_of(Register::Flags, rand() >> 8);
			writer.handler.processor.set_value_of(Register::X, rand() >> 8);
			writer.handler.processor.set_value_of(Register::Y, rand() >> 8);
			writer.handler.processor.set_value_of(Register::ProgramCounter, rand() >> 8);
			writer.handler.processor.set_value_of(Register::StackPointer, rand() >> 8);

			// Establish the opcode.
			writer.handler.setup(opcode);
			writer.write_initial_state(test);

			// Run to the second opcode fetch.
			try {
				writer.handler.processor.run_for(Cycles(100));
			} catch (const StopException &) {}

			// Dump all inventions as initial memory state.
			writer.write_initial_ram();

			// Dump final state.
			writer.end_test();
		}

		writer.end_operation();
	}
}

}

// MARK: - Existing test evaluator.

@interface WDC65816ComparativeTests : XCTestCase
@end

@implementation WDC65816ComparativeTests

// A generator for tests; not intended to be a permanent fixture.
- (void)testGenerate {
//	generate<CPU::MOS6502Esque::Type::TWDC65816>();
	test<CPU::MOS6502Esque::Type::TWDC65816>(SourceTests65816);
}

@end
