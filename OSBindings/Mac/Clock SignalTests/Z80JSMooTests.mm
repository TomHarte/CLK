//
//  Z80JSMooTests.cpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 19/9/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <optional>
#include "../../../Processors/Z80/Z80.hpp"

namespace {

// Tests are not duplicated into this repository due to their size;
// put them somewhere on your local system and provide the path here.
constexpr const char *TestPath = "/Users/thomasharte/Projects/jsmoo/misc/tests/GeneratedTests/z80/v1";

#define MapFields()	\
		Map(A, @"a");	Map(Flags, @"f");	Map(AFDash, @"af_");	\
		Map(B, @"b");	Map(C, @"c");		Map(BCDash, @"bc_");	\
		Map(D, @"d");	Map(E, @"e");		Map(DEDash, @"de_");	\
		Map(H, @"h");	Map(L, @"l");		Map(HLDash, @"hl_");	\
																	\
		Map(IX, @"ix");		Map(IY, @"iy");							\
		Map(IFF1, @"iff1");	Map(IFF2, @"iff2");						\
		Map(IM, @"im");												\
		Map(I, @"i");		Map(R, @"r");							\
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
		//
		// TEMPORARILY: DON'T COMPARE DidChangeFlags OR MemPtr.
#define Map(register, name)	\
	if(	\
		CPU::Z80::Register::register != CPU::Z80::Register::DidChangeFlags &&	\
		CPU::Z80::Register::register != CPU::Z80::Register::MemPtr &&	\
		z80_.get_value_of_register(CPU::Z80::Register::register) != [state[name] intValue]) {	\
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

	bool compare_bus_states(NSArray<NSArray *> *states) {
		auto capture = bus_records_.begin() + 1;

		int cycle = 0;
		for(NSArray *state in states) {
			// Extract proper bus state.
			const std::optional<uint16_t> address =
				[state[0] isKindOfClass:[NSNumber class]] ? std::optional<uint16_t>([state[0] intValue]) : std::nullopt;
			const std::optional<uint8_t> data =
				[state[1] isKindOfClass:[NSNumber class]] ? std::optional<uint8_t>([state[1] intValue]) : std::nullopt;

			NSString *const controls = state[2];
			const bool read = [controls characterAtIndex:0] != '-';
			const bool write = [controls characterAtIndex:1] != '-';
			const bool m1 = [controls characterAtIndex:2] != '-';
			const bool ioReq = [controls characterAtIndex:2] != '-';

			// Compare to captured state.
			bool failed = false;
			if(address != capture->address) {
				NSLog(@"Address mismatch after %d cycles", cycle);
				failed = true;
			}
			if(data != capture->data) {
				NSLog(@"Data mismatch after %d cycles", cycle);
				failed = true;
			}

			using Line = CPU::Z80::PartialMachineCycle::Line;
			if(read != bool(capture->lines & Line::RD)) {
				NSLog(@"Read line mismatch after %d cycles", cycle);
				failed = true;
			}
			if(write != bool(capture->lines & Line::WR)) {
				NSLog(@"Write line mismatch after %d cycles", cycle);
				failed = true;
			}
			if(m1 != bool(capture->lines & Line::M1)) {
				NSLog(@"M1 line mismatch after %d cycles", cycle);
				failed = true;
			}
			if(ioReq != bool(capture->lines & Line::IOREQ)) {
				NSLog(@"IOREQ line mismatch after %d cycles", cycle);
				failed = true;
			}

			if(failed) {
				return false;
			}

			// Advance.
			capture += 2;
			++cycle;
		}

		return true;
	}

	void run_for(int cycles) {
		z80_.run_for(HalfCycles(Cycles(cycles)));
		XCTAssertEqual(bus_records_.size(), cycles * 2);
	}

	struct BusRecord {
		std::optional<uint16_t> address = 0xffff;
		std::optional<uint8_t> data = 0xff;
		uint8_t lines = 0xff;

		BusRecord(std::optional<uint16_t> address, std::optional<uint8_t> data, uint8_t lines) :
			address(address), data(data), lines(lines) {}
	};

	HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {

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

		//
		// Capture bus activity.
		//
		const auto data = cycle.value ? std::optional<uint8_t>(*cycle.value) : std::nullopt;
		const auto address = cycle.address ? std::optional<uint16_t>(*cycle.address) : std::nullopt;
		const uint8_t* const bus = cycle.bus_state();
		for(int i = 0; i < cycle.length.as<int>(); i++) {
			bus_records_.emplace_back(address, data, bus[i]);
		}

		return HalfCycles(0);
	}

	const std::vector<BusRecord> &bus_records() const {
		return bus_records_;
	}

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

		std::vector<BusRecord> bus_records_;
};

}

@interface Z80JSMooTests : XCTestCase
@end

@implementation Z80JSMooTests

- (BOOL)applyTest:(NSDictionary *)test {
	// Log something.
//	NSLog(@"Test %@", test[@"name"]);

	// Seed Z80 and run to conclusion.
	auto z80 = std::make_unique<CapturingZ80>(test[@"initial"], test[@"ports"]);
	z80->run_for(int([test[@"cycles"] count]));
//	z80->run_for(15);

	// Check register and RAM state.
	return z80->compare_state(test[@"final"]) && z80->compare_bus_states(test[@"cycles"]);
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

	// TODO: switch to the below, or some approximation thereof.
	// Current issue: Z80 construction assumes something heading towards 500kb
	// of stack is available, and dispatch_apply seems to create an environment
	// much tighter than that.

//	__block BOOL allSucceeded = YES;
//
//	const size_t pageSize = 10;
//	const auto stepsPerBlock = size_t([tests count] / pageSize);
//	dispatch_apply(pageSize, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(size_t index) {
//		const size_t start = index * stepsPerBlock;
//		for(size_t c = start; c < start + stepsPerBlock; c++) {
//			assert(c < tests.count);
//			NSLog(@"%d begins %d", int(index), int(c));
//
//			allSucceeded &= [self applyTest:tests[c]];
//
//			if(!allSucceeded) {
//				NSLog(@"Failed at %@", tests[c][@"name"]);
//				return;
//			}
//		}
//    });

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
//		"ed a2.json",
//		"ed b2.json"
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
