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
#include <functional>

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

	// Parallelise as much as possible.
//	dispatch_apply(jsonContents.count,  DISPATCH_APPLY_AUTO, ^(size_t index) {
//		NSDictionary *const test = jsonContents[index];
//		if(![test isKindOfClass:[NSDictionary class]]) return;
//		[self testOperation:test];
//	});
	for(NSDictionary *test in jsonContents) {
		if(![test isKindOfClass:[NSDictionary class]]) continue;
		[self testOperation:test];
	}
}

- (void)testOperation:(NSDictionary *)test {
	// Only entries with a name are valid.
	NSString *const name = test[@"name"];
	if(!name) return;

//	if(![name isEqualToString:@"ORI 7c"]) return;

	// This is the test class for 68000 execution.
	struct Test68000: public CPU::MC68000::BusHandler {
		std::array<uint8_t, 16*1024*1024> ram;
		CPU::MC68000::Processor<Test68000, true, true> processor;
		std::function<void(void)> comparitor;

		Test68000() : processor(*this) {
		}

		void will_perform(uint32_t address, uint16_t opcode) {
			--instructions_remaining_;
			if(!instructions_remaining_) comparitor();
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
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

			XCTAssertEqual(state.data[c], [finalState[dX] integerValue], @"%@: D%d inconsistent", name, c);
			if(c < 7) {
				XCTAssertEqual(state.address[c], [finalState[aX] integerValue], @"%@: A%d inconsistent", name, c);
			}
		}
		XCTAssertEqual(state.supervisor_stack_pointer, [finalState[@"a7"] integerValue], @"%@: A7 inconsistent", name);
		XCTAssertEqual(state.user_stack_pointer, [finalState[@"usp"] integerValue], @"%@: USP inconsistent", name);
		XCTAssertEqual(state.status, [finalState[@"sr"] integerValue], @"%@: Status inconsistent", name);

		// Test final memory state.
		NSArray<NSNumber *> *const finalMemory = test[@"final memory"];
		NSEnumerator *enumerator = [finalMemory objectEnumerator];
		while(true) {
			NSNumber *const address = [enumerator nextObject];
			NSNumber *const value = [enumerator nextObject];

			if(!address || !value) break;
			XCTAssertEqual(test68000->ram[address.integerValue ^ 1], value.integerValue, @"%@: Memory at location %@ inconsistent", name, address);
		}
	};

	test68000->run_for_instructions(1, comparitor);
}

@end
