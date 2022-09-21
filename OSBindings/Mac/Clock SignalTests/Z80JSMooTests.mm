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
		bool failed = false;

		// Compare registers.
#define Map(register, name)	\
	if(z80_.get_value_of_register(CPU::Z80::Register::register) != [state[name] intValue]) {	\
		NSLog(@"Register %s should be %02x; is %02x", #register, [state[name] intValue], z80_.get_value_of_register(CPU::Z80::Register::register));	\
		failed = true;	\
	}

		MapFields()

#undef Map

		// Compare RAM.
		for(NSArray *byte in state[@"ram"]) {
			const int address = [byte[0] intValue] & 0xffff;
			const int value = [byte[1] intValue];

			if(ram_[address] != value) {
				NSLog(@"Value at address %04x should be %02x; is %02x", address, value, ram_[address]);
				failed = true;
			}
		}

		// Check ports.
		if(!ports_matched()) {
			NSLog(@"Mismatch in port activity");
			failed = true;
		}

		return !failed;
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
		@"2a.json",
/*		@"cb 0e.json",
		@"cb 16.json",
		@"cb 1e.json",
		@"cb 26.json",
		@"cb 2e.json",
		@"cb 36.json",
		@"cb 3e.json",
		@"cb 46.json",
		@"cb 4e.json",
		@"cb 56.json",
		@"cb 5e.json",
		@"cb 66.json",
		@"cb 6e.json",
		@"cb 76.json",
		@"cb 7e.json",
		@"cb 80.json",
		@"cb 81.json",
		@"cb 82.json",
		@"cb 83.json",
		@"cb 84.json",
		@"cb 85.json",
		@"cb 86.json",
		@"cb 87.json",
		@"cb 88.json",
		@"cb 89.json",
		@"cb 8a.json",
		@"cb 8b.json",
		@"cb 8c.json",
		@"cb 8d.json",
		@"cb 8e.json",
		@"cb 8f.json",
		@"cb 90.json",
		@"cb 91.json",
		@"cb 92.json",
		@"cb 93.json",
		@"cb 94.json",
		@"cb 95.json",
		@"cb 96.json",
		@"cb 97.json",
		@"cb 98.json",
		@"cb 99.json",
		@"cb 9a.json",
		@"cb 9b.json",
		@"cb 9c.json",
		@"cb 9d.json",
		@"cb 9e.json",
		@"cb 9f.json",
		@"cb a0.json",
		@"cb a1.json",
		@"cb a2.json",
		@"cb a3.json",
		@"cb a4.json",
		@"cb a5.json",
		@"cb a6.json",
		@"cb a7.json",
		@"cb a8.json",
		@"cb a9.json",
		@"cb aa.json",
		@"cb ab.json",
		@"cb ac.json",
		@"cb ad.json",
		@"cb ae.json",
		@"cb af.json",
		@"cb b0.json",
		@"cb b1.json",
		@"cb b2.json",
		@"cb b3.json",
		@"cb b4.json",
		@"cb b5.json",
		@"cb b6.json",
		@"cb b7.json",
		@"cb b8.json",
		@"cb b9.json",
		@"cb ba.json",
		@"cb bb.json",
		@"cb bc.json",
		@"cb bd.json",
		@"cb be.json",
		@"cb bf.json",
		@"cb c0.json",
		@"cb c1.json",
		@"cb c2.json",
		@"cb c3.json",
		@"cb c4.json",
		@"cb c5.json",
		@"cb c6.json",
		@"cb c7.json",
		@"cb c8.json",
		@"cb c9.json",
		@"cb ca.json",
		@"cb cb.json",
		@"cb cc.json",
		@"cb cd.json",
		@"cb ce.json",
		@"cb cf.json",
		@"cb d0.json",
		@"cb d1.json",
		@"cb d2.json",
		@"cb d3.json",
		@"cb d4.json",
		@"cb d5.json",
		@"cb d6.json",
		@"cb d7.json",
		@"cb d8.json",
		@"cb d9.json",
		@"cb da.json",
		@"cb db.json",
		@"cb dc.json",
		@"cb dd.json",
		@"cb de.json",
		@"cb df.json",
		@"cb e0.json",
		@"cb e1.json",
		@"cb e2.json",
		@"cb e3.json",
		@"cb e4.json",
		@"cb e5.json",
		@"cb e6.json",
		@"cb e7.json",
		@"cb e8.json",
		@"cb e9.json",
		@"cb ea.json",
		@"cb eb.json",
		@"cb ec.json",
		@"cb ed.json",
		@"cb ee.json",
		@"cb ef.json",
		@"cb f0.json",
		@"cb f1.json",
		@"cb f2.json",
		@"cb f3.json",
		@"cb f4.json",
		@"cb f5.json",
		@"cb f6.json",
		@"cb f7.json",
		@"cb f8.json",
		@"cb f9.json",
		@"cb fa.json",
		@"cb fb.json",
		@"cb fc.json",
		@"cb fd.json",
		@"cb fe.json",
		@"cb ff.json",
		@"dd 2a.json",
		@"ed 40.json",
		@"ed 46.json",
		@"ed 47.json",
		@"ed 48.json",
		@"ed 4b.json",
		@"ed 4e.json",
		@"ed 56.json",
		@"ed 57.json",
		@"ed 5b.json",
		@"ed 5e.json",
		@"ed 66.json",
		@"ed 6b.json",
		@"ed 6e.json",
		@"ed 6f.json",
		@"ed 71.json",
		@"ed 76.json",
		@"ed 7b.json",
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
		@"ed bb.json",
		@"fd 2a.json",
		@"dd cb __ 80.json",
		@"dd cb __ 81.json",
		@"dd cb __ 82.json",
		@"dd cb __ 83.json",
		@"dd cb __ 84.json",
		@"dd cb __ 85.json",
		@"dd cb __ 86.json",
		@"dd cb __ 87.json",
		@"dd cb __ 88.json",
		@"dd cb __ 89.json",
		@"dd cb __ 8a.json",
		@"dd cb __ 8b.json",
		@"dd cb __ 8c.json",
		@"dd cb __ 8d.json",
		@"dd cb __ 8e.json",
		@"dd cb __ 8f.json",
		@"dd cb __ 90.json",
		@"dd cb __ 91.json",
		@"dd cb __ 92.json",
		@"dd cb __ 93.json",
		@"dd cb __ 94.json",
		@"dd cb __ 95.json",
		@"dd cb __ 96.json",
		@"dd cb __ 97.json",
		@"dd cb __ 98.json",
		@"dd cb __ 99.json",
		@"dd cb __ 9a.json",
		@"dd cb __ 9b.json",
		@"dd cb __ 9c.json",
		@"dd cb __ 9d.json",
		@"dd cb __ 9e.json",
		@"dd cb __ 9f.json",
		@"dd cb __ a0.json",
		@"dd cb __ a1.json",
		@"dd cb __ a2.json",
		@"dd cb __ a3.json",
		@"dd cb __ a4.json",
		@"dd cb __ a5.json",
		@"dd cb __ a6.json",
		@"dd cb __ a7.json",
		@"dd cb __ a8.json",
		@"dd cb __ a9.json",
		@"dd cb __ aa.json",
		@"dd cb __ ab.json",
		@"dd cb __ ac.json",
		@"dd cb __ ad.json",
		@"dd cb __ ae.json",
		@"dd cb __ af.json",
		@"dd cb __ b0.json",
		@"dd cb __ b1.json",
		@"dd cb __ b2.json",
		@"dd cb __ b3.json",
		@"dd cb __ b4.json",
		@"dd cb __ b5.json",
		@"dd cb __ b6.json",
		@"dd cb __ b7.json",
		@"dd cb __ b8.json",
		@"dd cb __ b9.json",
		@"dd cb __ ba.json",
		@"dd cb __ bb.json",
		@"dd cb __ bc.json",
		@"dd cb __ bd.json",
		@"dd cb __ be.json",
		@"dd cb __ bf.json",
		@"dd cb __ c0.json",
		@"dd cb __ c1.json",
		@"dd cb __ c2.json",
		@"dd cb __ c3.json",
		@"dd cb __ c4.json",
		@"dd cb __ c5.json",
		@"dd cb __ c6.json",
		@"dd cb __ c7.json",
		@"dd cb __ c8.json",
		@"dd cb __ c9.json",
		@"dd cb __ ca.json",
		@"dd cb __ cb.json",
		@"dd cb __ cc.json",
		@"dd cb __ cd.json",
		@"dd cb __ ce.json",
		@"dd cb __ cf.json",
		@"dd cb __ d0.json",
		@"dd cb __ d1.json",
		@"dd cb __ d2.json",
		@"dd cb __ d3.json",
		@"dd cb __ d4.json",
		@"dd cb __ d5.json",
		@"dd cb __ d6.json",
		@"dd cb __ d7.json",
		@"dd cb __ d8.json",
		@"dd cb __ d9.json",
		@"dd cb __ da.json",
		@"dd cb __ db.json",
		@"dd cb __ dc.json",
		@"dd cb __ dd.json",
		@"dd cb __ de.json",
		@"dd cb __ df.json",
		@"dd cb __ e0.json",
		@"dd cb __ e1.json",
		@"dd cb __ e2.json",
		@"dd cb __ e3.json",
		@"dd cb __ e4.json",
		@"dd cb __ e5.json",
		@"dd cb __ e6.json",
		@"dd cb __ e7.json",
		@"dd cb __ e8.json",
		@"dd cb __ e9.json",
		@"dd cb __ ea.json",
		@"dd cb __ eb.json",
		@"dd cb __ ec.json",
		@"dd cb __ ed.json",
		@"dd cb __ ee.json",
		@"dd cb __ ef.json",
		@"dd cb __ f0.json",
		@"dd cb __ f1.json",
		@"dd cb __ f2.json",
		@"dd cb __ f3.json",
		@"dd cb __ f4.json",
		@"dd cb __ f5.json",
		@"dd cb __ f6.json",
		@"dd cb __ f7.json",
		@"dd cb __ f8.json",
		@"dd cb __ f9.json",
		@"dd cb __ fa.json",
		@"dd cb __ fb.json",
		@"dd cb __ fc.json",
		@"dd cb __ fd.json",
		@"dd cb __ fe.json",
		@"dd cb __ ff.json",
		@"fd cb __ 80.json",
		@"fd cb __ 81.json",
		@"fd cb __ 82.json",
		@"fd cb __ 83.json",
		@"fd cb __ 84.json",
		@"fd cb __ 85.json",
		@"fd cb __ 86.json",
		@"fd cb __ 87.json",
		@"fd cb __ 88.json",
		@"fd cb __ 89.json",
		@"fd cb __ 8a.json",
		@"fd cb __ 8b.json",
		@"fd cb __ 8c.json",
		@"fd cb __ 8d.json",
		@"fd cb __ 8e.json",
		@"fd cb __ 8f.json",
		@"fd cb __ 90.json",
		@"fd cb __ 91.json",
		@"fd cb __ 92.json",
		@"fd cb __ 93.json",
		@"fd cb __ 94.json",
		@"fd cb __ 95.json",
		@"fd cb __ 96.json",
		@"fd cb __ 97.json",
		@"fd cb __ 98.json",
		@"fd cb __ 99.json",
		@"fd cb __ 9a.json",
		@"fd cb __ 9b.json",
		@"fd cb __ 9c.json",
		@"fd cb __ 9d.json",
		@"fd cb __ 9e.json",
		@"fd cb __ 9f.json",
		@"fd cb __ a0.json",
		@"fd cb __ a1.json",
		@"fd cb __ a2.json",
		@"fd cb __ a3.json",
		@"fd cb __ a4.json",
		@"fd cb __ a5.json",
		@"fd cb __ a6.json",
		@"fd cb __ a7.json",
		@"fd cb __ a8.json",
		@"fd cb __ a9.json",
		@"fd cb __ aa.json",
		@"fd cb __ ab.json",
		@"fd cb __ ac.json",
		@"fd cb __ ad.json",
		@"fd cb __ ae.json",
		@"fd cb __ af.json",
		@"fd cb __ b0.json",
		@"fd cb __ b1.json",
		@"fd cb __ b2.json",
		@"fd cb __ b3.json",
		@"fd cb __ b4.json",
		@"fd cb __ b5.json",
		@"fd cb __ b6.json",
		@"fd cb __ b7.json",
		@"fd cb __ b8.json",
		@"fd cb __ b9.json",
		@"fd cb __ ba.json",
		@"fd cb __ bb.json",
		@"fd cb __ bc.json",
		@"fd cb __ bd.json",
		@"fd cb __ be.json",
		@"fd cb __ bf.json",
		@"fd cb __ c0.json",
		@"fd cb __ c1.json",
		@"fd cb __ c2.json",
		@"fd cb __ c3.json",
		@"fd cb __ c4.json",
		@"fd cb __ c5.json",
		@"fd cb __ c6.json",
		@"fd cb __ c7.json",
		@"fd cb __ c8.json",
		@"fd cb __ c9.json",
		@"fd cb __ ca.json",
		@"fd cb __ cb.json",
		@"fd cb __ cc.json",
		@"fd cb __ cd.json",
		@"fd cb __ ce.json",
		@"fd cb __ cf.json",
		@"fd cb __ d0.json",
		@"fd cb __ d1.json",
		@"fd cb __ d2.json",
		@"fd cb __ d3.json",
		@"fd cb __ d4.json",
		@"fd cb __ d5.json",
		@"fd cb __ d6.json",
		@"fd cb __ d7.json",
		@"fd cb __ d8.json",
		@"fd cb __ d9.json",
		@"fd cb __ da.json",
		@"fd cb __ db.json",
		@"fd cb __ dc.json",
		@"fd cb __ dd.json",
		@"fd cb __ de.json",
		@"fd cb __ df.json",
		@"fd cb __ e0.json",
		@"fd cb __ e1.json",
		@"fd cb __ e2.json",
		@"fd cb __ e3.json",
		@"fd cb __ e4.json",
		@"fd cb __ e5.json",
		@"fd cb __ e6.json",
		@"fd cb __ e7.json",
		@"fd cb __ e8.json",
		@"fd cb __ e9.json",
		@"fd cb __ ea.json",
		@"fd cb __ eb.json",
		@"fd cb __ ec.json",
		@"fd cb __ ed.json",
		@"fd cb __ ee.json",
		@"fd cb __ ef.json",
		@"fd cb __ f0.json",
		@"fd cb __ f1.json",
		@"fd cb __ f2.json",
		@"fd cb __ f3.json",
		@"fd cb __ f4.json",
		@"fd cb __ f5.json",
		@"fd cb __ f6.json",
		@"fd cb __ f7.json",
		@"fd cb __ f8.json",
		@"fd cb __ f9.json",
		@"fd cb __ fa.json",
		@"fd cb __ fb.json",
		@"fd cb __ fc.json",
		@"fd cb __ fd.json",
		@"fd cb __ fe.json",
		@"fd cb __ ff.json"*/
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
