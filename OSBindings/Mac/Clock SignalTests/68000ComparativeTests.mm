//
//  68000ComparativeTests.cpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 14/12/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Processors/68000/68000.hpp"
#include "../../../InstructionSets/M68k/Executor.hpp"

#include <array>
#include <memory>
#include <functional>

//#define USE_EXISTING_IMPLEMENTATION

@interface M68000ComparativeTests : XCTestCase
@end

@implementation M68000ComparativeTests {
	NSSet<NSString *> *_fileSet;
	NSSet<NSString *> *_testSet;

	NSMutableSet<NSString *> *_failures;
	NSMutableArray<NSNumber *> *_failingOpcodes;
}

// New implementation verified against (i.e. has the same failures as the old, likely all BCD related):
//
//	add_sub
//	addi_subi_cmpi
//	eor_and_or
//	addq_subq
//	addx_subx
//	bcc
//	cmp
//	dbcc_scc
//	eori_andi_ori
//	lea
//	lslr_aslr_roxlr_rolr
//	move_tofrom_srccr
//	move
//	movep

// Issues to fix:
//
//	LINK A7, in which the post-operation writeback overwrites
//	the modified value.
//
// Do I need a dedicated LINKA7 operation?
//
// This affects link_unlk

// Skipped for now, for implying a more granular decoder:
//
//	btst_bchg_bclr_bset
//	chk
//
// And for uncertainty around the test result status register correctness:
//
//	divu_divs
//	eor_and_or	(which invokes BCD)
//	exg			(also BCD)
//	chk
//
// And because possibly my old CHK is pushing the wrong program counter?
//
//	ext.json

- (void)setUp {
	// To limit tests run to a subset of files and/or of tests, uncomment and fill in below.
	_fileSet = [NSSet setWithArray:@[@"movem.json"]];
	_testSet = [NSSet setWithArray:@[@"MOVEM 00a8 (0)"]];
//	_fileSet = [NSSet setWithArray:@[@"jmp_jsr.json"]];
//	_testSet = [NSSet setWithArray:@[@"CHK 41a8"]];
}

- (void)testAll {
	// These will accumulate a list of failing tests and associated opcodes.
	_failures = [[NSMutableSet alloc] init];
	_failingOpcodes = [[NSMutableArray alloc] init];

	// Get the full list of available test files.
	NSBundle *const bundle = [NSBundle bundleForClass:[self class]];
	NSArray<NSURL *> *const tests = [bundle URLsForResourcesWithExtension:@"json" subdirectory:@"68000 Comparative Tests"];

	// Issue each test file.
	for(NSURL *url in tests) {
		// Compare against a file set if one has been supplied.
		if(_fileSet && ![_fileSet containsObject:[url lastPathComponent]]) continue;
		NSLog(@"Testing %@", url);
		[self testJSONAtURL:url];
	}

	// Output a summary of failures.
	NSLog(@"Total: %@", @(_failures.count));
	NSLog(@"Failures: %@", _failures);
	NSLog(@"Failing opcodes:");
	for(NSNumber *number in _failingOpcodes) {
		NSLog(@"%04x", number.intValue);
	}
}

- (void)testJSONAtURL:(NSURL *)url {
	// Read the nominated file and parse it as JSON.
	NSData *const data = [NSData dataWithContentsOfURL:url];
	NSError *error;
	NSArray *const jsonContents = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];

	if(!data || error || ![jsonContents isKindOfClass:[NSArray class]]) {
		return;
	}

	// Perform each dictionary in the array as a test.
	for(NSDictionary *test in jsonContents) {
		if(![test isKindOfClass:[NSDictionary class]]) continue;
#ifdef USE_EXISTING_IMPLEMENTATION
		[self testOperationClassic:test];
#else
		[self testOperationExecutor:test];
#endif
	}
}

- (void)testOperationClassic:(NSDictionary *)test {
	// Only entries with a name are valid.
	NSString *const name = test[@"name"];
	if(!name) return;

	// Compare against a test set if one has been supplied.
	if(_testSet && ![_testSet containsObject:name]) return;

	// This is the test class for 68000 execution.
	struct Test68000: public CPU::MC68000::BusHandler {
		std::array<uint8_t, 16*1024*1024> ram;
		CPU::MC68000::Processor<Test68000, true, true> processor;
		std::function<void(void)> comparitor;

		Test68000() : processor(*this) {
		}

		void will_perform(uint32_t, uint16_t) {
			--instructions_remaining_;
			if(!instructions_remaining_) comparitor();
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int) {
			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				cycle.apply(&ram[cycle.host_endian_byte_address()]);
			}
			return HalfCycles(0);
		}

		void run_for_instructions(int instructions, const std::function<void(void)> &compare) {
			instructions_remaining_ = instructions + 1;	// i.e. run up to the will_perform of the instruction after.
			comparitor = std::move(compare);
			while(instructions_remaining_) {
				processor.run_for(HalfCycles(2));
			}
		}

		private:
			int instructions_remaining_;
	};
	auto uniqueTest68000 = std::make_unique<Test68000>();
	auto test68000 = uniqueTest68000.get();
	memset(test68000->ram.data(), 0xce, test68000->ram.size());

	{
		// Apply initial memory state.
		NSArray<NSNumber *> *const initialMemory = test[@"initial memory"];
		NSEnumerator<NSNumber *> *enumerator = [initialMemory objectEnumerator];
		while(true) {
			NSNumber *const address = [enumerator nextObject];
			NSNumber *const value = [enumerator nextObject];

			if(!address || !value) break;
			test68000->ram[address.integerValue ^ 1] = value.integerValue;	// Effect a short-resolution endianness swap.
		}

		// Apply initial processor state.
		NSDictionary *const initialState = test[@"initial state"];
		auto state = test68000->processor.get_state();
		for(int c = 0; c < 8; ++c) {
			const NSString *dX = [@"d" stringByAppendingFormat:@"%d", c];
			const NSString *aX = [@"a" stringByAppendingFormat:@"%d", c];

			state.data[c] = uint32_t([initialState[dX] integerValue]);
			if(c < 7)
				state.address[c] = uint32_t([initialState[aX] integerValue]);
		}
		state.supervisor_stack_pointer = uint32_t([initialState[@"a7"] integerValue]);
		state.user_stack_pointer = uint32_t([initialState[@"usp"] integerValue]);
		state.status = [initialState[@"sr"] integerValue];
		test68000->processor.set_state(state);
	}

	// Run the thing.
	const auto comparitor = [=] {
		// Test the end state.
		NSDictionary *const finalState = test[@"final state"];
		const auto state = test68000->processor.get_state();
		for(int c = 0; c < 8; ++c) {
			const NSString *dX = [@"d" stringByAppendingFormat:@"%d", c];
			const NSString *aX = [@"a" stringByAppendingFormat:@"%d", c];

			if(state.data[c] != [finalState[dX] integerValue]) [_failures addObject:name];
			if(c < 7 && state.address[c] != [finalState[aX] integerValue]) [_failures addObject:name];

			XCTAssertEqual(state.data[c], [finalState[dX] integerValue], @"%@: D%d inconsistent", name, c);
			if(c < 7) {
				XCTAssertEqual(state.address[c], [finalState[aX] integerValue], @"%@: A%d inconsistent", name, c);
			}
		}
		if(state.supervisor_stack_pointer != [finalState[@"a7"] integerValue]) [_failures addObject:name];
		if(state.user_stack_pointer != [finalState[@"usp"] integerValue]) [_failures addObject:name];
		if(state.status != [finalState[@"sr"] integerValue]) [_failures addObject:name];

		XCTAssertEqual(state.supervisor_stack_pointer, [finalState[@"a7"] integerValue], @"%@: A7 inconsistent", name);
		XCTAssertEqual(state.user_stack_pointer, [finalState[@"usp"] integerValue], @"%@: USP inconsistent", name);
		XCTAssertEqual(state.status, [finalState[@"sr"] integerValue], @"%@: Status inconsistent", name);
		XCTAssertEqual(state.program_counter - 4, [finalState[@"pc"] integerValue], @"%@: Program counter inconsistent", name);

		// Test final memory state.
		NSArray<NSNumber *> *const finalMemory = test[@"final memory"];
		NSEnumerator *enumerator = [finalMemory objectEnumerator];
		while(true) {
			NSNumber *const address = [enumerator nextObject];
			NSNumber *const value = [enumerator nextObject];

			if(!address || !value) break;
			XCTAssertEqual(test68000->ram[address.integerValue ^ 1], value.integerValue, @"%@: Memory at location %@ inconsistent", name, address);
			if(test68000->ram[address.integerValue ^ 1] != value.integerValue) [_failures addObject:name];
		}

		// Consider collating extra detail.
		if([_failures containsObject:name]) {
			[_failingOpcodes addObject:@((test68000->ram[0x101] << 8) | test68000->ram[0x100])];
		}
	};

	test68000->run_for_instructions(1, comparitor);
}

- (void)testOperationExecutor:(NSDictionary *)test {
	// Only entries with a name are valid.
	NSString *const name = test[@"name"];
	if(!name) return;

	// Compare against a test set if one has been supplied.
	if(_testSet && ![_testSet containsObject:name]) return;

	// This is the test class for 68000 execution.
	struct Test68000 {
		std::array<uint8_t, 16*1024*1024> ram;
		InstructionSet::M68k::Executor<InstructionSet::M68k::Model::M68000, Test68000> processor;

		Test68000() : processor(*this) {
		}

		void run_for_instructions(int instructions) {
			processor.run_for_instructions(instructions);
		}

		// Initial test-case implementation:
		// do a very sedate read and write.

		template <typename IntT> IntT read(uint32_t address) {
			if constexpr (sizeof(IntT) == 1) {
				return IntT(ram[address & 0xffffff]);
			}

			if constexpr (sizeof(IntT) == 2) {
				return IntT(
					(ram[address & 0xffffff] << 8) |
					ram[(address+1) & 0xffffff]
				);
			}

			if constexpr (sizeof(IntT) == 4) {
				return IntT(
					(ram[address & 0xffffff] << 24) |
					(ram[(address+1) & 0xffffff] << 16) |
					(ram[(address+2) & 0xffffff] << 8) |
					ram[(address+3) & 0xffffff]
				);
			}
			return 0;
		}

		template <typename IntT> void write(uint32_t address, IntT value) {
			if constexpr (sizeof(IntT) == 1) {
				ram[address & 0xffffff] = uint8_t(value);
			}

			if constexpr (sizeof(IntT) == 2) {
				ram[address & 0xffffff] = uint8_t(value >> 8);
				ram[(address+1) & 0xffffff] = uint8_t(value);
			}

			if constexpr (sizeof(IntT) == 4) {
				ram[address & 0xffffff] = uint8_t(value >> 24);
				ram[(address+1) & 0xffffff] = uint8_t(value >> 16);
				ram[(address+2) & 0xffffff] = uint8_t(value >> 8);
				ram[(address+3) & 0xffffff] = uint8_t(value);
			}
		}
	};
	auto uniqueTest68000 = std::make_unique<Test68000>();
	auto test68000 = uniqueTest68000.get();
	memset(test68000->ram.data(), 0xce, test68000->ram.size());

	{
		// Apply initial memory state.
		NSArray<NSNumber *> *const initialMemory = test[@"initial memory"];
		NSEnumerator<NSNumber *> *enumerator = [initialMemory objectEnumerator];
		while(true) {
			NSNumber *const address = [enumerator nextObject];
			NSNumber *const value = [enumerator nextObject];

			if(!address || !value) break;
			test68000->ram[address.integerValue] = value.integerValue;
		}

		// Apply initial processor state.
		NSDictionary *const initialState = test[@"initial state"];
		auto state = test68000->processor.get_state();
		for(int c = 0; c < 8; ++c) {
			const NSString *dX = [@"d" stringByAppendingFormat:@"%d", c];
			const NSString *aX = [@"a" stringByAppendingFormat:@"%d", c];

			state.data[c] = uint32_t([initialState[dX] integerValue]);
			if(c < 7)
				state.address[c] = uint32_t([initialState[aX] integerValue]);
		}
		state.supervisor_stack_pointer = uint32_t([initialState[@"a7"] integerValue]);
		state.user_stack_pointer = uint32_t([initialState[@"usp"] integerValue]);
		state.status = [initialState[@"sr"] integerValue];
		state.program_counter = uint32_t([initialState[@"pc"] integerValue]);
		test68000->processor.set_state(state);
	}

	// Run the thing.
	test68000->run_for_instructions(1);

	// Test the end state.
	NSDictionary *const finalState = test[@"final state"];
	const auto state = test68000->processor.get_state();
	for(int c = 0; c < 8; ++c) {
		const NSString *dX = [@"d" stringByAppendingFormat:@"%d", c];
		const NSString *aX = [@"a" stringByAppendingFormat:@"%d", c];

		if(state.data[c] != [finalState[dX] integerValue]) [_failures addObject:name];
		if(c < 7 && state.address[c] != [finalState[aX] integerValue]) [_failures addObject:name];

		XCTAssertEqual(state.data[c], [finalState[dX] integerValue], @"%@: D%d inconsistent", name, c);
		if(c < 7) {
			XCTAssertEqual(state.address[c], [finalState[aX] integerValue], @"%@: A%d inconsistent", name, c);
		}
	}
	if(state.supervisor_stack_pointer != [finalState[@"a7"] integerValue]) [_failures addObject:name];
	if(state.user_stack_pointer != [finalState[@"usp"] integerValue]) [_failures addObject:name];
	if(state.status != [finalState[@"sr"] integerValue]) [_failures addObject:name];

	XCTAssertEqual(state.supervisor_stack_pointer, [finalState[@"a7"] integerValue], @"%@: A7 inconsistent", name);
	XCTAssertEqual(state.user_stack_pointer, [finalState[@"usp"] integerValue], @"%@: USP inconsistent", name);
	XCTAssertEqual(state.status, [finalState[@"sr"] integerValue], @"%@: Status inconsistent", name);
	XCTAssertEqual(state.program_counter, [finalState[@"pc"] integerValue], @"%@: Program counter inconsistent", name);

	// Test final memory state.
	NSArray<NSNumber *> *const finalMemory = test[@"final memory"];
	NSEnumerator *enumerator = [finalMemory objectEnumerator];
	while(true) {
		NSNumber *const address = [enumerator nextObject];
		NSNumber *const value = [enumerator nextObject];

		if(!address || !value) break;
		XCTAssertEqual(test68000->ram[address.integerValue], value.integerValue, @"%@: Memory at location %@ inconsistent", name, address);
		if(test68000->ram[address.integerValue] != value.integerValue) [_failures addObject:name];
	}

	// Consider collating extra detail.
	if([_failures containsObject:name]) {
		[_failingOpcodes addObject:@((test68000->ram[0x100] << 8) | test68000->ram[0x101])];
	}
}

@end
