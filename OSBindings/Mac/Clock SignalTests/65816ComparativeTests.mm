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

[[maybe_unused]] const char *SourceTests65816 = "/Users/thomasharte/Projects/65816/v1";
constexpr auto MaxTestLength = Cycles(65537 * 7);	// MVN and MVP can copy up to 65536 bytes; each byte takes 7 cycles.

template <CPU::MOS6502Esque::Type type>
constexpr bool is_block_copy(const uint8_t opcode) {
	return type == CPU::MOS6502Esque::Type::TWDC65816 && (opcode == 0x54 || opcode == 0x44);
}

struct StopException {};

template <CPU::MOS6502Esque::Type type>
struct Lines {
	bool vda = false;
	bool vpa = false;
	bool vpb = false;
	bool read = false;
	bool wait = false;

	bool emulation = false;
	bool memory_size = false;
	bool index_size = false;
	bool memory_lock = false;

	bool operator == (const Lines &rhs) const {
		if constexpr(has(type, CPU::MOS6502Esque::Register::EmulationFlag)) {
			return
				vda == rhs.vda &&
				vpa == rhs.vpa &&
				vpb == rhs.vpb &&
				(!wait && read) == (!rhs.wait && rhs.read) &&
				wait == rhs.wait &&

				emulation == rhs.emulation &&
				memory_size == rhs.memory_size &&
				index_size == rhs.index_size &&
				memory_lock == rhs.memory_lock;
		} else {
			return read == rhs.read;
		}
	}
};

template <CPU::MOS6502Esque::Type type>
struct BusHandler: public CPU::MOS6502Esque::BusHandlerT<type> {
	using AddressType = typename CPU::MOS6502Esque::BusHandlerT<type>::AddressType;

	std::unordered_map<AddressType, uint8_t> ram;			// Current RAM contents.
	std::unordered_map<AddressType, uint8_t> inventions;	// All values invented by the handler, i.e. places where a read took place but no value was yet stored.
	std::unordered_map<AddressType, uint8_t> reservoir;		// A wholly optional reservoir of seed values from which the inventions can pull.

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
					const auto suggested = reservoir.find(address);
					if(suggested != reservoir.end()) {
						cycle.value = *value = suggested->second;
					} else {
						cycle.value = *value = uint8_t(rand() >> 8);
					}
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

	AddressType current_pc() const {
		using Register = CPU::MOS6502Esque::Register;
		return
			AddressType(
				processor.value_of(Register::ProgramCounter) |
				(processor.value_of(Register::ProgramBank) << 16)
			);
	}

	void reset() {
		ram.clear();
		inventions.clear();
		reservoir.clear();
		cycles.clear();
		pc_overshoot = 0;
		processor.restart_operation_fetch();
		initial_pc = std::nullopt;
	}

	void setup() {
		// Test for whether MVP or MVN is known to be coming.
		allow_pc_repetition = false;
		const auto suggestion = reservoir.find(current_pc());
		if(suggestion != reservoir.end()) {
			if(is_block_copy<type>(suggestion->second)) {
				allow_pc_repetition = true;

				// Limit A to up to 1000, as otherwise the MVN and MVP tests are just interminable.
				using Register = CPU::MOS6502Esque::Register;
				processor.set_value_of(Register::A, processor.value_of(Register::A) % 1000);
			}
		}
	}

	void setup(const uint8_t opcode) {
		const auto pc = current_pc();
		reservoir[pc] = inventions[pc] = ram[pc] = opcode;
		setup();
	}

	void posit(const uint32_t address, const uint8_t value) {
		reservoir[address] = value;
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

		Lines<type> lines() const {
			Lines<type> result;

			using BusOperation = CPU::MOS6502Esque::BusOperation;
			switch(operation) {
				using enum BusOperation;

				case Read:					result.read = result.vda = true;				break;
				case ReadOpcode:			result.read = result.vda = result.vpa = true;	break;
				case ReadProgram:			result.read = result.vpa = true;				break;
				case ReadVector:			result.read = result.vpb = result.vda = true;	break;
				case InternalOperationRead:	result.read = true;								break;

				case Write:					result.vda = true;				break;
				case InternalOperationWrite:								break;

				case None:
				case Ready:					result.wait = true;				break;

				default:
					assert(false);
			}

			if(!result.wait) {
				using ExtendedBusOutput = CPU::WDC65816::ExtendedBusOutput;
				result.emulation = extended_bus & ExtendedBusOutput::Emulation;
				result.memory_size = extended_bus & ExtendedBusOutput::MemorySize;
				result.index_size = extended_bus & ExtendedBusOutput::IndexSize;
				result.memory_lock = extended_bus & ExtendedBusOutput::MemoryLock;
			}

			return result;
		}
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

/*!
	Contains logic to formulate a JSON entry from the various parts of a test result.
*/
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

	void set_target_name(NSString *const targetName) {
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
		handler.reset();
	}

	void write_initial_state(const int test) {
		// Dump initial state.
		if(has_emulation) {
			fprintf(target, "{ \"name\": \"%02x %c %d\", ", opcode_, is_emulated_ ? 'e' : 'n', test + 1);
		} else {
			fprintf(target, "{ \"name\": \"%02x %d\", ", opcode_, test + 1);
		}
		fprintf(target, "\"initial\": {");
		print_registers<has_emulation>(target, handler.processor, 0, std::nullopt);
	}

	void write_initial_state(NSString *const name, const std::optional<int> s_override) {
		fprintf(target, "{ \"name\": \"%s\", ", name.UTF8String);
		fprintf(target, "\"initial\": {");
		print_registers<has_emulation>(target, handler.processor, 0, s_override);
	}

	void write_initial_ram(NSArray *const prior = nil) {
		print_ram(target, handler.inventions, prior);
	}

	void end_test(NSArray *const prior_ram = nil) {
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

			const auto lines = cycle.lines();
			if(has_emulation) {
				fprintf(target, "\"%c%c%c%c%c%c%c%c\"]",
					lines.vda ? 'd' : '-',
					lines.vpa ? 'p' : '-',
					lines.vpb ? 'v' : '-',
					lines.wait ? '-' : (lines.read ? 'r' : 'w'),
					lines.emulation ? 'e' : '-',
					lines.memory_size ? 'm' : '-',
					lines.index_size ? 'x' : '-',
					lines.memory_lock ? 'l' : '-'
				);
			} else {
				if(lines.read) {
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


// MARK: - Regression runner.

/// Regression tests against @c source outputting a new set of JSON files that differ anywhere
/// the current 65816 differs in result from the regression test.
template <CPU::MOS6502Esque::Type type> void test(const char *source) {
	TestWriter<type> writer;

	NSString *sourcePath = [NSString stringWithUTF8String:source];
	NSArray<NSString *> *const files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:sourcePath error:nil];
	for(NSString *file in files) @autoreleasepool {
		writer.set_target_name(file);

		NSArray *tests =
			[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:[sourcePath stringByAppendingPathComponent:file]] options:0 error:nil];
		bool report_errors = true;
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

			// Posit RAM values that were in the previous version of this test.
			NSArray *initialRAM = initial[@"ram"];
			for(NSArray *value in initialRAM) {
				writer.handler.posit([value[0] intValue], [value[1] intValue]);
			}

			writer.handler.setup();
			NSString *const name = test[@"name"];
			writer.write_initial_state(name, [initial[@"s"] intValue]);

			// Run to the second opcode fetch.
			try {
				writer.handler.processor.run_for(MaxTestLength);
			} catch (const StopException &) {}

			// Dump all inventions as initial memory state. Provide old RAM array to match ordering.
			writer.write_initial_ram(initialRAM);

			// Dump final state.
			NSDictionary *const final_state = test[@"final"];
			NSArray<NSArray<NSNumber *> *> *const final_ram = final_state[@"ram"];
			writer.end_test(final_ram);

			// Actually do the regression test.
			if(report_errors) {
				//
				// Test registers.
				//
				bool registers_equal = true;
				if(writer.has_emulation) {
					registers_equal &= writer.handler.processor.value_of(Register::DataBank) == [final_state[@"dbr"] intValue];
					registers_equal &= writer.handler.processor.value_of(Register::ProgramBank) == [final_state[@"pbr"] intValue];
					registers_equal &= writer.handler.processor.value_of(Register::Direct) == [final_state[@"d"] intValue];
					registers_equal &= writer.handler.processor.value_of(Register::EmulationFlag) == [final_state[@"e"] intValue];
				}
				registers_equal &= writer.handler.processor.value_of(Register::A) == [final_state[@"a"] intValue];
				registers_equal &= writer.handler.processor.value_of(Register::Flags) == [final_state[@"p"] intValue];
				registers_equal &= writer.handler.processor.value_of(Register::X) == [final_state[@"x"] intValue];
				registers_equal &= writer.handler.processor.value_of(Register::Y) == [final_state[@"y"] intValue];
				registers_equal &= writer.handler.processor.value_of(Register::StackPointer) == [final_state[@"s"] intValue];

				const auto final_pc = (writer.handler.processor.value_of(Register::ProgramCounter) + writer.handler.pc_overshoot) & 65535;
				registers_equal &= final_pc == [final_state[@"pc"] intValue];

				XCTAssertEqual(registers_equal, true, @"End registers diverged in test %@", name);

				//
				// Test RAM.
				//
				bool ram_equal = true;
				for(NSArray<NSNumber *> *value in final_ram) {
					const auto address = [value[0] intValue];
					const auto data = [value[1] intValue];

					const auto comparison = writer.handler.ram.find(address);
					ram_equal &= comparison != writer.handler.ram.end() && comparison->second == data;
				}

				XCTAssertEqual(ram_equal, true, @"Final RAM state diverged in test %@", name);

				//
				// Test bus activity.
				//
				NSArray *const cycles = test[@"cycles"];
				NSEnumerator *enumerator = [cycles objectEnumerator];
				bool cycles_equal = cycles.count == writer.handler.cycles.size();
				if(cycles_equal) {
					for(const auto &cycle: writer.handler.cycles) {
						NSArray *const comparator = [enumerator nextObject];

						// Compare the two numeric fields.
						const auto compare_number = [&](const size_t index, auto &&expected) {
							// NSNull is a singleton; it's valid to compare by value.
							if(comparator[index] == [NSNull null]) {
								cycles_equal &= expected == std::nullopt;
							} else {
								const auto found = [comparator[index] intValue];
								cycles_equal &= expected.value_or(found ^ 1) == found;
							}
						};
						compare_number(0, cycle.address);
						compare_number(1, cycle.value);

						// Compare bus state.
						Lines<type> lines;
						NSString *linesText = comparator[2];
						if(writer.has_emulation) {
							if(linesText.length == 8) {
								const char *const linesCharacters = linesText.UTF8String;
								lines.vda = linesCharacters[0] != '-';
								lines.vpa = linesCharacters[1] != '-';
								lines.vpb = linesCharacters[2] != '-';
								lines.read = linesCharacters[3] == 'r';
								lines.emulation = linesCharacters[4] != '-';
								lines.memory_size = linesCharacters[5] != '-';
								lines.index_size = linesCharacters[6] != '-';
								lines.memory_lock = linesCharacters[7] != '-';
							} else {
								cycles_equal = false;
							}
						} else {
							lines.read = [linesText isEqual:@"read"];
						}

						cycles_equal &= lines == cycle.lines();
					}
				}

				XCTAssertEqual(cycles_equal, true, @"Bus cycles diverged in test %@", name);

				if(!registers_equal || !ram_equal || !cycles_equal) {
					NSLog(@"Not reporting any further failures for file %@", file);
					report_errors = false;
				}
			}
		}

		writer.end_operation();
	}
}

// MARK: - Clean-slate test generator.

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

		// Provide substantially fewer tests for MVP and MVN; they're not actually that interesting and
		// they're lengthy.
		for(int test = 0; test < (is_block_copy<type>(opcode) ? 100 : 10'000); test++) {
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
				writer.handler.processor.run_for(MaxTestLength);
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

// Compares current implementation to existing tests.
- (void)testRegression {
	test<CPU::MOS6502Esque::Type::TWDC65816>(SourceTests65816);
}

// Generates an entirely new set of tests.
//- (void)testGenerate {
//	generate<CPU::MOS6502Esque::Type::TWDC65816>();
//}

@end
