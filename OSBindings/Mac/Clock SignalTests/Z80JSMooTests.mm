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

struct CapturingZ80: public CPU::Z80::BusHandler {

	CapturingZ80(NSDictionary *state) : z80_(*this) {
		z80_.reset_power_on();

		// Set registers.
		z80_.set_value_of_register(CPU::Z80::Register::A, [state[@"a"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::Flags, [state[@"f"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::AFDash, [state[@"af_"] intValue]);

		z80_.set_value_of_register(CPU::Z80::Register::B, [state[@"b"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::C, [state[@"c"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::BCDash, [state[@"bc_"] intValue]);

		z80_.set_value_of_register(CPU::Z80::Register::D, [state[@"d"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::E, [state[@"e"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::DEDash, [state[@"de_"] intValue]);

		z80_.set_value_of_register(CPU::Z80::Register::H, [state[@"h"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::L, [state[@"l"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::HLDash, [state[@"hl_"] intValue]);

		z80_.set_value_of_register(CPU::Z80::Register::IFF1, [state[@"iff1"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::IFF2, [state[@"iff2"] intValue]);

		z80_.set_value_of_register(CPU::Z80::Register::IM, [state[@"im"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::Refresh, [state[@"r"] intValue]);

		z80_.set_value_of_register(CPU::Z80::Register::IX, [state[@"ix"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::IY, [state[@"iy"] intValue]);

		z80_.set_value_of_register(CPU::Z80::Register::ProgramCounter, [state[@"pc"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::StackPointer, [state[@"sp"] intValue]);
		z80_.set_value_of_register(CPU::Z80::Register::MemPtr, [state[@"wz"] intValue]);

		/*
			Not used:

				EI (duplicative of IFF1?)
				p
				q
		*/

		for(NSArray *value in state[@"ram"]) {
			ram_[[value[0] intValue]] = [value[1] intValue];
		}
	}

	void compare_state(NSDictionary *state) {
		// Compare RAM.
		for(NSArray *value in state[@"ram"]) {
			XCTAssertEqual(ram_[[value[0] intValue]], [value[1] intValue]);
		}
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

	const std::vector<BusRecord> &bus_records() const {
		return bus_records_;
	}


	private:
		CPU::Z80::Processor<CapturingZ80, false, false> z80_;
		uint8_t ram_[65536];

		std::vector<BusRecord> bus_records_;
};

}

@interface Z80JSMooTests : XCTestCase
@end

@implementation Z80JSMooTests

- (void)applyTest:(NSDictionary *)test {
	// Seed Z80 and run to conclusion.
	CapturingZ80 z80(test[@"initial"]);
	z80.run_for(int([test[@"cycles"] count]));

	// Check register and RAM state.
	z80.compare_state(test[@"final"]);

	// TODO: check bus cycles.
}

- (void)applyTests:(NSString *)path {
	NSArray<NSDictionary *> *const tests =
		[NSJSONSerialization JSONObjectWithData:
			[NSData dataWithContentsOfFile:path]
		options:0
		error:nil];

	XCTAssertNotNil(tests);

	for(NSDictionary *test in tests) {
		[self applyTest:test];
	}
}

- (void)testAll {
	// Get a list of everything in the 'TestPath' directory.
	NSError *error;
	NSString *const testPath = @(TestPath);
	NSArray<NSString *> *const sources =
		[[NSFileManager defaultManager] contentsOfDirectoryAtPath:testPath error:&error];

	// Treat lack of a local copy of these tests as a non-failing condition.
	if(error || ![sources count]) {
		NSLog(@"No tests found at %s; not testing, not failing", TestPath);
		return;
	}

	// Apply tests one by one.
	for(NSString *source in sources) {
		if(![[source pathExtension] isEqualToString:@"json"]) {
			NSLog(@"Skipping %@", source);
			continue;
		}

		NSLog(@"Testing %@", source);
		[self applyTests:[testPath stringByAppendingPathComponent:source]];
	}
}
@end
