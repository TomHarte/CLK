//
//  Z80JSMooTests.cpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 19/9/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Processors/Z80/Z80.hpp"

namespace {

// Tests are not duplicated into this repository due to their size;
// put them somewhere on your local system and provide the path here.
constexpr const char *TestPath = "/Users/thomasharte/Projects/jsmoo-main/misc/tests/GeneratedTests/z80/v1";

#define MapFields()	\
		Map(A, @"a");	Map(Flags, @"f");	Map(AFDash, @"af_");	\
		Map(B, @"b");	Map(C, @"c");		Map(BCDash, @"bc_");	\
		Map(D, @"d");	Map(E, @"e");		Map(DEDash, @"de_");	\
		Map(H, @"h");	Map(L, @"l");		Map(HLDash, @"hl_");	\
																	\
		Map(IX, @"ix");		Map(IY, @"iy");							\
		Map(IFF1, @"iff1");	Map(IFF2, @"iff2");						\
		Map(IM, @"im");		Map(Refresh, @"r");						\
																	\
		Map(ProgramCounter, @"pc");									\
		Map(StackPointer, @"sp");									\
		Map(MemPtr, @"wz");

		/*
			Not used:

				EI (duplicative of IFF1?)
				p
				q
		*/

struct CapturingZ80: public CPU::Z80::BusHandler {

	CapturingZ80(NSDictionary *state) : z80_(*this) {
		z80_.reset_power_on();

		// Set registers.
#define Map(register, name)	z80_.set_value_of_register(CPU::Z80::Register::register, [state[name] intValue])

		MapFields();

#undef Map

		for(NSArray *byte in state[@"ram"]) {
			const int address = [byte[0] intValue] & 0xffff;
			const int value = [byte[1] intValue];
			ram_[address] = value;
		}
	}

	bool compare_state(NSDictionary *state) {
		// Compare registers.
#define Map(register, name)	\
	if(z80_.get_value_of_register(CPU::Z80::Register::register) != [state[name] intValue]) {	\
		NSLog(@"Register %s should be %02x; is %02x", #register, [state[name] intValue], z80_.get_value_of_register(CPU::Z80::Register::register));	\
		return false;	\
	}

		MapFields()

#undef Map

		// Compare RAM.
		for(NSArray *byte in state[@"ram"]) {
			const int address = [byte[0] intValue] & 0xffff;
			const int value = [byte[1] intValue];

			if(ram_[address] != value) {
				NSLog(@"Value at address %04x should be %02x; is %02x", address, value, ram_[address]);
				return false;
			}
		}

		return true;
	}

	void run_for(int cycles) {
		z80_.run_for(HalfCycles(Cycles(cycles)));
//		XCTAssertEqual(bus_records_.size(), cycles * 2);
	}

	/// A record of the state of the address bus, MREQ, IOREQ and RFSH lines,
	/// upon every clock transition.
	struct BusRecord {
//		uint16_t address = 0xffff;
//		bool mreq = false, ioreq = false, refresh = false;
	};

	HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
		//
		// TODO: capture bus activity.
		//


		//
		// Do the actual action.
		//
		switch(cycle.operation) {
			default: break;

			case CPU::Z80::PartialMachineCycle::Read:
			case CPU::Z80::PartialMachineCycle::ReadOpcode:
				*cycle.value = ram_[*cycle.address];
			break;

			case CPU::Z80::PartialMachineCycle::Write:
				ram_[*cycle.address] = *cycle.value;
			break;
		}

		return HalfCycles(0);
	}

//	const std::vector<BusRecord> &bus_records() const {
//		return bus_records_;
//	}


	private:
		CPU::Z80::Processor<CapturingZ80, false, false> z80_;
		uint8_t ram_[65536];

//		std::vector<BusRecord> bus_records_;
};

}

@interface Z80JSMooTests : XCTestCase
@end

@implementation Z80JSMooTests

- (BOOL)applyTest:(NSDictionary *)test {
	// Log something.
//	NSLog(@"Test %@", test[@"name"]);

	// Seed Z80 and run to conclusion.
	CapturingZ80 z80(test[@"initial"]);
	z80.run_for(int([test[@"cycles"] count]));

	// Check register and RAM state.
	return z80.compare_state(test[@"final"]);

	// TODO: check bus cycles.
}

- (BOOL)applyTests:(NSString *)path {
	NSArray<NSDictionary *> *const tests =
		[NSJSONSerialization JSONObjectWithData:
			[NSData dataWithContentsOfFile:path]
		options:0
		error:nil];

	XCTAssertNotNil(tests);

	BOOL allSucceeded = YES;
	for(NSDictionary *test in tests) {
		allSucceeded &= [self applyTest:test];

		if(!allSucceeded) {
			NSLog(@"Failed at %@", test[@"name"]);
			return NO;
		}
	}
	return allSucceeded;
}

- (void)testAll {
	// Get a list of everything in the 'TestPath' directory.
	NSError *error;
	NSString *const testPath = @(TestPath);
	NSArray<NSString *> *const sources =
		[[NSFileManager defaultManager] contentsOfDirectoryAtPath:testPath error:&error];

	// Optional: a permit list; leave empty to allow all tests.
	NSSet<NSString *> *permitList = [NSSet setWithArray:@[
		@"ed 71.json",
		@"fd cb __ 5f.json",
		@"dd cb __ 7a.json",
		@"fd cb __ 67.json",
		@"cb 1e.json",
		@"dd cb __ 40.json",
		@"cb e6.json",
		@"fd cb __ 71.json",
		@"dd cb __ 60.json",
		@"ed b3.json",
		@"fd cb __ 51.json",
		@"dd 2a.json",
		@"fd cb __ 47.json",
		@"cb 86.json",
		@"cb 3e.json",
		@"ed ab.json",
		@"ed 47.json",
		@"dd cb __ 5a.json",
		@"fd cb __ 7f.json",
		@"dd cb __ 4f.json",
		@"ed 50.json",
		@"ed 46.json",
		@"fd cb __ 6a.json",
		@"ed a8.json",
		@"dd cb __ 77.json",
		@"cb c6.json",
		@"dd 3f.json",
		@"ed b2.json",
		@"cb 7e.json",
		@"fd cb __ 50.json",
		@"dd cb __ 61.json",
		@"db.json",
		@"d9.json",
		@"cb 5e.json",
		@"fd cb __ 70.json",
		@"37.json",
		@"dd cb __ 41.json",
		@"cb a6.json",
		@"dd cb __ 57.json",
		@"e3.json",
		@"ed 66.json",
		@"fd cb __ 4a.json",
		@"dd cb __ 6f.json",
		@"ed 70.json",
		@"cb 2e.json",
		@"fd cb __ 57.json",
		@"cb 96.json",
		@"fd cb __ 41.json",
		@"cb f6.json",
		@"dd cb __ 70.json",
		@"ed 6f.json",
		@"2a.json",
		@"fd cb __ 6f.json",
		@"ed 57.json",
		@"dd cb __ 4a.json",
		@"dd cb __ 6a.json",
		@"ed bb.json",
		@"fd cb __ 4f.json",
		@"cb d6.json",
		@"fd cb __ 61.json",
		@"ed a3.json",
		@"dd cb __ 50.json",
		@"cb 0e.json",
		@"fd cb __ 77.json",
		@"dd e3.json",
		@"dd cb __ 47.json",
		@"dd cb __ 51.json",
		@"cb 4e.json",
		@"fd cb __ 60.json",
		@"ed a2.json",
		@"dd cb __ 7f.json",
		@"ed 60.json",
		@"dd 37.json",
		@"ed b8.json",
		@"fd cb __ 5a.json",
		@"ed 76.json",
		@"3f.json",
		@"fd cb __ 7a.json",
		@"ed 56.json",
		@"dd db.json",
		@"dd cb __ 5f.json",
		@"ed 40.json",
		@"dd d9.json",
		@"dd cb __ 71.json",
		@"cb 6e.json",
		@"fd cb __ 40.json",
		@"dd cb __ 67.json",
		@"cb b6.json",
		@"fd cb __ 7b.json",
		@"cb ee.json",
		@"fd cb __ 79.json",
		@"dd cb __ 48.json",
		@"dd cb __ 4c.json",
		@"cb 16.json",
		@"fd cb __ 6d.json",
		@"dd cb __ 72.json",
		@"fd cb __ 43.json",
		@"fd cb __ 55.json",
		@"dd cb __ 64.json",
		@"ed 7b.json",
		@"fd cb __ 75.json",
		@"dd cb __ 44.json",
		@"ed 5b.json",
		@"dd cb __ 52.json",
		@"fd cb __ 63.json",
		@"cb 36.json",
		@"cb 8e.json",
		@"fd 3f.json",
		@"fd cb __ 4d.json",
		@"fd cb __ 59.json",
		@"fd cb __ 5b.json",
		@"dd cb __ 6c.json",
		@"dd cb __ 68.json",
		@"dd cb __ 69.json",
		@"dd cb __ 6b.json",
		@"fd 2a.json",
		@"ed ba.json",
		@"cb 76.json",
		@"fd cb __ 5c.json",
		@"fd cb __ 58.json",
		@"dd cb __ 7d.json",
		@"cb ce.json",
		@"ed a0.json",
		@"fd cb __ 62.json",
		@"ed 4e.json",
		@"dd cb __ 53.json",
		@"dd cb __ 45.json",
		@"ed 58.json",
		@"fd cb __ 74.json",
		@"ed 78.json",
		@"dd cb __ 65.json",
		@"fd cb __ 54.json",
		@"fd cb __ 42.json",
		@"ed 6e.json",
		@"dd cb __ 73.json",
		@"cb ae.json",
		@"dd cb __ 5d.json",
		@"dd cb __ 4b.json",
		@"dd cb __ 49.json",
		@"fd cb __ 78.json",
		@"cb 56.json",
		@"fd cb __ 7c.json",
		@"fd cb __ 73.json",
		@"dd cb __ 42.json",
		@"fd d9.json",
		@"dd cb __ 54.json",
		@"ed 4b.json",
		@"fd db.json",
		@"fd cb __ 65.json",
		@"dd cb __ 7c.json",
		@"dd cb __ 78.json",
		@"fd cb __ 49.json",
		@"cb fe.json",
		@"fd cb __ 4b.json",
		@"cb 9e.json",
		@"fd cb __ 5d.json",
		@"cb 26.json",
		@"fd cb __ 7d.json",
		@"cb 06.json",
		@"fd e3.json",
		@"dd cb __ 58.json",
		@"dd cb __ 5c.json",
		@"fd cb __ 6b.json",
		@"cb de.json",
		@"fd cb __ 69.json",
		@"dd cb __ 74.json",
		@"ed 6b.json",
		@"fd cb __ 45.json",
		@"fd 37.json",
		@"fd cb __ 53.json",
		@"dd cb __ 62.json",
		@"ed 7e.json",
		@"dd cb __ 63.json",
		@"fd cb __ 52.json",
		@"ed b0.json",
		@"fd cb __ 44.json",
		@"ed 68.json",
		@"dd cb __ 75.json",
		@"fd cb __ 68.json",
		@"cb 46.json",
		@"fd cb __ 6c.json",
		@"ed aa.json",
		@"dd cb __ 5b.json",
		@"dd cb __ 59.json",
		@"dd cb __ 4d.json",
		@"dd cb __ 6d.json",
		@"cb be.json",
		@"cb 66.json",
		@"fd cb __ 4c.json",
		@"fd cb __ 48.json",
		@"dd cb __ 79.json",
		@"dd cb __ 7b.json",
		@"fd cb __ 64.json",
		@"dd cb __ 55.json",
		@"ed 48.json",
		@"ed 5e.json",
		@"dd cb __ 43.json",
		@"fd cb __ 72.json"
	]];

	// Treat lack of a local copy of these tests as a non-failing condition.
	if(error || ![sources count]) {
		NSLog(@"No tests found at %s; not testing, not failing", TestPath);
		return;
	}

	// Apply tests one by one.
	NSMutableArray *failures = [[NSMutableArray alloc] init];
	for(NSString *source in sources) {
		if(![[source pathExtension] isEqualToString:@"json"]) {
			NSLog(@"Skipping %@", source);
			continue;
		}

		// Skip if: (i) there is a permit list; and (ii) this file isn't on it.
		if([permitList count] && ![permitList containsObject:source]) {
			continue;
		}

		NSLog(@"Testing %@", source);
		if(![self applyTests:[testPath stringByAppendingPathComponent:source]]) {
			NSLog(@"Failed");
			[failures addObject:source];
		}
	}
	NSLog(@"Files with failures were: %@", failures);

	XCTAssertEqual([failures count], 0);
}
@end
