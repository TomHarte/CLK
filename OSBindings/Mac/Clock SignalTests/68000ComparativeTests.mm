//
//  68000ComparativeTests.cpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 14/12/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Processors/68000/68000.hpp"

#include <array>
#include <memory>

@interface M68000ComparativeTests : XCTestCase
@end

@implementation M68000ComparativeTests

- (void)testAll {
	NSBundle *const bundle = [NSBundle bundleForClass:[self class]];
	NSArray<NSURL *> *const tests = [bundle URLsForResourcesWithExtension:@"json" subdirectory:@"68000 Comparative Tests"];
	for(NSURL *url in tests) {
		[self testJSONAtURL:url];
	}
}

- (void)testJSONAtURL:(NSURL *)url {
	NSData *const data = [NSData dataWithContentsOfURL:url];
	NSError *error;
	NSArray *const jsonContents = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];

	if(!data || error || ![jsonContents isKindOfClass:[NSArray class]]) {
		return;
	}

	for(NSDictionary *test in jsonContents) {
		if(![test isKindOfClass:[NSDictionary class]]) continue;
		[self testOperation:test];
	}
}

- (void)testOperation:(NSDictionary *)test {
	// Only entries with a name are valid.
	NSString *const name = test[@"name"];
	if(!name) return;

	// This is the test class for 68000 execution.
	struct Test68000: public CPU::MC68000::BusHandler {
		std::array<uint8_t, 16*1024*1024> ram;
		CPU::MC68000::Processor<Test68000, true, true> processor;

		Test68000() : processor(*this) {
		}

		void will_perform(uint32_t address, uint16_t opcode) {
			--instructions_remaining_;
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				cycle.apply(&ram[cycle.host_endian_byte_address()]);
			}
			return HalfCycles(0);
		}

		void run_for_instructions(int instructions) {
			instructions_remaining_ = instructions + 1;	// i.e. run up to the will_perform of the instruction after.
			while(instructions_remaining_) {
				processor.run_for(HalfCycles(2));
			}
		}

		private:
			int instructions_remaining_;
	};
	auto test68000 = std::make_unique<Test68000>();

	// Apply initial memory state.
	NSArray<NSNumber *> *const initialMemory = test[@"initial memory"];
	NSEnumerator<NSNumber *> *enumerator = [initialMemory objectEnumerator];
	while(true) {
		NSNumber *const address = [enumerator nextObject];
		NSNumber *const value = [enumerator nextObject];

		if(!address || !value) break;
		test68000->ram[address.intValue ^ 1] = value.intValue;	// Effect a short-resolution endianness swap.
	}

	// Apply initial processor state.
	NSDictionary *const initialState = test[@"initial state"];
	auto state = test68000->processor.get_state();
	for(int c = 0; c < 8; ++c) {
		const NSString *dX = [@"d" stringByAppendingFormat:@"%d", c];
		const NSString *aX = [@"a" stringByAppendingFormat:@"%d", c];

		state.data[c] = [initialState[dX] intValue];
		if(c < 7)
			state.address[c] = [initialState[aX] intValue];
	}
	state.supervisor_stack_pointer = [initialState[@"a7"] intValue];
	state.user_stack_pointer = [initialState[@"usp"] intValue];
	state.status = [initialState[@"sr"] intValue];
	test68000->processor.set_state(state);

	// Run the thing.
	test68000->run_for_instructions(1);

	// Test the end state.
	NSDictionary *const finalState = test[@"final state"];
	state = test68000->processor.get_state();
	for(int c = 0; c < 8; ++c) {
		const NSString *dX = [@"d" stringByAppendingFormat:@"%d", c];
		const NSString *aX = [@"a" stringByAppendingFormat:@"%d", c];

		XCTAssertEqual(state.data[c], [finalState[dX] intValue], @"%@: D%d inconsistent", name, c);
		if(c < 7) {
			XCTAssertEqual(state.address[c], [finalState[aX] intValue], @"%@: A%d inconsistent", name, c);
		}
	}
	XCTAssertEqual(state.supervisor_stack_pointer, [finalState[@"a7"] intValue], @"%@: A7 inconsistent", name);
	XCTAssertEqual(state.user_stack_pointer, [finalState[@"usp"] intValue], @"%@: USP inconsistent", name);
	XCTAssertEqual(state.status, [finalState[@"sr"] intValue], @"%@: Status inconsistent", name);

	// Test final memory state.
	NSArray<NSNumber *> *const finalMemory = test[@"final memory"];
	enumerator = [finalMemory objectEnumerator];
	while(true) {
		NSNumber *const address = [enumerator nextObject];
		NSNumber *const value = [enumerator nextObject];

		if(!address || !value) break;
		XCTAssertEqual(test68000->ram[address.intValue ^ 1], value.intValue, @"%@: Memory at location %@ inconsistent", name, address);
	}
}

@end
