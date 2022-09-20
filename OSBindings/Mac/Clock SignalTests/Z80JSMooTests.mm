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
		Map(MemPtr, @"wz");											\
		Map(DidChangeFlags, @"q");

		/*
			Not used:

				EI (duplicative of IFF1?)
				p
		*/

struct CapturingZ80: public CPU::Z80::BusHandler {

	CapturingZ80(NSDictionary *state, NSArray *port_activity) : z80_(*this) {
		z80_.reset_power_on();

		// Set registers.
#define Map(register, name)	z80_.set_value_of_register(CPU::Z80::Register::register, [state[name] intValue])
		MapFields();
#undef Map

		// Populate RAM.
		for(NSArray *byte in state[@"ram"]) {
			const int address = [byte[0] intValue] & 0xffff;
			const int value = [byte[1] intValue];
			ram_[address] = value;
		}

		// Capture expected port activity.
		for(NSArray *item in port_activity) {
			expected_port_accesses_.emplace_back([item[0] intValue], [item[1] intValue], [item[2] isEqualToString:@"r"]);
		}
		next_port_ = expected_port_accesses_.begin();
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

		// Check ports.
		if(!ports_matched()) {
			NSLog(@"Mismatch in port activity");
			return false;
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

			case CPU::Z80::PartialMachineCycle::Input:
				if(next_port_ != expected_port_accesses_.end() && next_port_->is_read && next_port_->address == *cycle.address) {
					*cycle.value = next_port_->value;
					++next_port_;
				} else {
					ports_matched_ = false;
					*cycle.value = 0xff;
				}
			break;

			case CPU::Z80::PartialMachineCycle::Output:
				if(next_port_ != expected_port_accesses_.end() && !next_port_->is_read && next_port_->address == *cycle.address) {
					ports_matched_ &= *cycle.value == next_port_->value;
					++next_port_;
				} else {
					ports_matched_ = false;
				}
			break;
		}

		return HalfCycles(0);
	}

//	const std::vector<BusRecord> &bus_records() const {
//		return bus_records_;
//	}

	bool ports_matched() const {
		return ports_matched_ && next_port_ == expected_port_accesses_.end();
	}

	private:
		CPU::Z80::Processor<CapturingZ80, false, false> z80_;
		uint8_t ram_[65536];

		struct PortAccess {
			const uint16_t address = 0;
			const uint8_t value = 0;
			const bool is_read = false;

			PortAccess(uint16_t a, uint8_t v, bool r) : address(a), value(v), is_read(r) {}
		};
		std::vector<PortAccess> expected_port_accesses_;
		std::vector<PortAccess>::iterator next_port_;
		bool ports_matched_ = true;

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
	CapturingZ80 z80(test[@"initial"], test[@"ports"]);
	z80.run_for(int([test[@"cycles"] count]));
//	z80.run_for(15);

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
//		@"2a.json",		// Memptr
//		@"d9.json",		// EXX doesn't seem to update HL'?
//		@"e3.json",		// EX (SP), HL; cycle count is too short.

//		@"cb 06.json",	// RLC (HL) [, etc]; cycle count is too short
//		@"cb 0e.json",
//		@"cb 16.json",
//		@"cb 1e.json",
//		@"cb 26.json",
//		@"cb 2e.json",
//		@"cb 36.json",
//		@"cb 3e.json",
//		@"cb 46.json",
//		@"cb 4e.json",
//		@"cb 56.json",
//		@"cb 5e.json",
//		@"cb 66.json",
//		@"cb 6e.json",
//		@"cb 76.json",
//		@"cb 7e.json",
//		@"cb 86.json",
//		@"cb 8e.json",
//		@"cb 96.json",
//		@"cb 9e.json",
//		@"cb a6.json",
//		@"cb ae.json",
//		@"cb b6.json",
//		@"cb be.json",
//		@"cb c6.json",
//		@"cb ce.json",
//		@"cb d6.json",
//		@"cb de.json",
//		@"cb e6.json",
//		@"cb ee.json",
//		@"cb f6.json",
//		@"cb fe.json",

//		@"dd d9.json",
//		@"dd e3.json",

//		@"ed 46.json",	// IM 0; at least refresh mismatch. Incremented twice here, once in tests.
/*		@"ed 47.json",
		@"ed 4e.json",
		@"ed 56.json",
		@"ed 57.json",
		@"ed 5e.json",
		@"ed 66.json",
		@"ed 6e.json",
		@"ed 6f.json",
		@"ed 76.json",
		@"ed 7e.json",
		@"ed a0.json",
		@"ed a2.json",
		@"ed a3.json",
		@"ed a8.json",
		@"ed aa.json",
		@"ed ab.json",
		@"ed b0.json",
		@"ed b2.json",
		@"ed b3.json",
		@"ed b8.json",
		@"ed ba.json",
		@"ed bb.json", */
/*		@"fd d9.json",
		@"fd e3.json",
		@"dd cb __ 40.json",
		@"dd cb __ 41.json",
		@"dd cb __ 42.json",
		@"dd cb __ 43.json",
		@"dd cb __ 44.json",
		@"dd cb __ 45.json",
		@"dd cb __ 47.json",
		@"dd cb __ 48.json",
		@"dd cb __ 49.json",
		@"dd cb __ 4a.json",
		@"dd cb __ 4b.json",
		@"dd cb __ 4c.json",
		@"dd cb __ 4d.json",
		@"dd cb __ 4f.json",
		@"dd cb __ 50.json",
		@"dd cb __ 51.json",
		@"dd cb __ 52.json",
		@"dd cb __ 53.json",
		@"dd cb __ 54.json",
		@"dd cb __ 55.json",
		@"dd cb __ 57.json",
		@"dd cb __ 58.json",
		@"dd cb __ 59.json",
		@"dd cb __ 5a.json",
		@"dd cb __ 5b.json",
		@"dd cb __ 5c.json",
		@"dd cb __ 5d.json",
		@"dd cb __ 5f.json",
		@"dd cb __ 60.json",
		@"dd cb __ 61.json",
		@"dd cb __ 62.json",
		@"dd cb __ 63.json",
		@"dd cb __ 64.json",
		@"dd cb __ 65.json",
		@"dd cb __ 67.json",
		@"dd cb __ 68.json",
		@"dd cb __ 69.json",
		@"dd cb __ 6a.json",
		@"dd cb __ 6b.json",
		@"dd cb __ 6c.json",
		@"dd cb __ 6d.json",
		@"dd cb __ 6f.json",
		@"dd cb __ 70.json",
		@"dd cb __ 71.json",
		@"dd cb __ 72.json",
		@"dd cb __ 73.json",
		@"dd cb __ 74.json",
		@"dd cb __ 75.json",
		@"dd cb __ 77.json",
		@"dd cb __ 78.json",
		@"dd cb __ 79.json",
		@"dd cb __ 7a.json",
		@"dd cb __ 7b.json",
		@"dd cb __ 7c.json",
		@"dd cb __ 7d.json",
		@"dd cb __ 7f.json",
		@"fd cb __ 40.json",
		@"fd cb __ 41.json",
		@"fd cb __ 42.json",
		@"fd cb __ 43.json",
		@"fd cb __ 44.json",
		@"fd cb __ 45.json",
		@"fd cb __ 47.json",
		@"fd cb __ 48.json",
		@"fd cb __ 49.json",
		@"fd cb __ 4a.json",
		@"fd cb __ 4b.json",
		@"fd cb __ 4c.json",
		@"fd cb __ 4d.json",
		@"fd cb __ 4f.json",
		@"fd cb __ 50.json",
		@"fd cb __ 51.json",
		@"fd cb __ 52.json",
		@"fd cb __ 53.json",
		@"fd cb __ 54.json",
		@"fd cb __ 55.json",
		@"fd cb __ 57.json",
		@"fd cb __ 58.json",
		@"fd cb __ 59.json",
		@"fd cb __ 5a.json",
		@"fd cb __ 5b.json",
		@"fd cb __ 5c.json",
		@"fd cb __ 5d.json",
		@"fd cb __ 5f.json",
		@"fd cb __ 60.json",
		@"fd cb __ 61.json",
		@"fd cb __ 62.json",
		@"fd cb __ 63.json",
		@"fd cb __ 64.json",
		@"fd cb __ 65.json",
		@"fd cb __ 67.json",
		@"fd cb __ 68.json",
		@"fd cb __ 69.json",
		@"fd cb __ 6a.json",
		@"fd cb __ 6b.json",
		@"fd cb __ 6c.json",
		@"fd cb __ 6d.json",
		@"fd cb __ 6f.json",
		@"fd cb __ 70.json",
		@"fd cb __ 71.json",
		@"fd cb __ 72.json",
		@"fd cb __ 73.json",
		@"fd cb __ 74.json",
		@"fd cb __ 75.json",
		@"fd cb __ 77.json",
		@"fd cb __ 78.json",
		@"fd cb __ 79.json",
		@"fd cb __ 7a.json",
		@"fd cb __ 7b.json",
		@"fd cb __ 7c.json",
		@"fd cb __ 7d.json",
		@"fd cb __ 7f.json"*/
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

	[failures sortUsingComparator:^NSComparisonResult(NSString *obj1, NSString *obj2) {
		if([obj1 length] < [obj2 length]) {
			return NSOrderedAscending;
		}
		if([obj2 length] < [obj1 length]) {
			return NSOrderedDescending;
		}

		return [obj1 compare:obj2];
	}];
	NSLog(@"Files with failures were: %@", failures);

	XCTAssertEqual([failures count], 0);
}
@end
