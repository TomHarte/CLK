//
//  AmigaBlitterTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 25/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "Blitter.hpp"

#include <unordered_map>
#include <vector>

namespace Amiga {
/// An empty stub to satisfy Amiga::Blitter's inheritance from Amiga::DMADevice;
struct Chipset {
	// Hyper ugliness: make a gross assumption about the effect of
	// the only call the Blitter will make into the Chipset, i.e.
	// that it will write something but do nothing more.
	//
	// Bonus ugliness: assume the real Chipset struct is 1kb in
	// size, at most.
	uint8_t _[1024];
};

};

namespace {

using WriteVector = std::vector<std::pair<uint32_t, uint16_t>>;

}

@interface AmigaBlitterTests: XCTestCase
@end

@implementation AmigaBlitterTests

- (BOOL)verifyWrites:(WriteVector &)writes blitter:(Amiga::Blitter &)blitter ram:(uint16_t *)ram approximateLocation:(NSInteger)approximateLocation {
	// Run for however much time the Blitter wants.
	while(blitter.get_status() & 0x4000) {
		blitter.advance_dma();
	}

	// Some blits will write the same address twice
	// (e.g. by virtue of an appropriate modulo), but
	// this unit test is currently able to verify the
	// final result only. So count number of accesses per
	// address up front in order only to count the
	// final ones below.
	std::unordered_map<int, int> access_counts;
	for(const auto &write: writes) {
		++access_counts[write.first];
	}

	for(const auto &write: writes) {
		auto &count = access_counts[write.first];
		--count;
		if(count) continue;

		XCTAssertEqual(ram[write.first >> 1], write.second, @"Didn't find %04x at address %08x; found %04x instead, somewhere before line %ld", write.second, write.first, ram[write.first >> 1], (long)approximateLocation);

		// For now, indicate only the first failure.
		if(ram[write.first >> 1] != write.second) {
			return NO;
		}
	}
	writes.clear();
	return YES;
}

- (void)testCase:(NSString *)name {
	uint16_t ram[256 * 1024]{};
	Amiga::Chipset nonChipset;
	Amiga::Blitter blitter(nonChipset, ram, 256 * 1024);

	NSURL *const traceURL = [[NSBundle bundleForClass:[self class]] URLForResource:name withExtension:@"json" subdirectory:@"Amiga Blitter Tests"];
	NSData *const traceData = [NSData dataWithContentsOfURL:traceURL];
	NSArray *const trace = [NSJSONSerialization JSONObjectWithData:traceData options:0 error:nil];

	// Step 1 in developing my version of the Blitter is to make sure that I understand
	// the logic; as a result the first implementation is going to be a magical thing that
	// completes all Blits in a single cycle.
	//
	// Therefore I've had to bodge my way around the trace's record of reads and writes by
	// accumulating all writes into a blob and checking them en massse at the end of a blit
	// (as detected by any register work in between memory accesses, since Kickstart 1.3
	// doesn't do anything off-book).
	enum class State {
		AwaitingWrites,
		LoggingWrites
	} state = State::AwaitingWrites;

	WriteVector writes;
	BOOL hasFailed = NO;

	NSInteger arrayEntry = -1;
	for(NSArray *const event in trace) {
		++arrayEntry;
		if(hasFailed) break;

		NSString *const type = event[0];
		const NSInteger param1 = [event[1] integerValue];

		if([type isEqualToString:@"cread"] || [type isEqualToString:@"bread"] || [type isEqualToString:@"aread"]) {
			XCTAssert(param1 < sizeof(ram) - 1);
			ram[param1 >> 1] = [event[2] integerValue];
			state = State::LoggingWrites;
			continue;
		}
		if([type isEqualToString:@"write"]) {
			const uint16_t value = uint16_t([event[2] integerValue]);

			if(writes.empty() || writes.back().first != param1) {
				writes.push_back(std::make_pair(uint32_t(param1), value));
			} else {
				writes.back().second = value;
			}
			state = State::LoggingWrites;
			continue;
		}

		// Hackaround for testing my magical all-at-once Blitter is here.
		if(state == State::LoggingWrites) {
			if(![self verifyWrites:writes blitter:blitter ram:ram approximateLocation:arrayEntry]) {
				hasFailed = YES;
				break;
			}
			state = State::AwaitingWrites;
		}
		// Hack ends here.


		if([type isEqualToString:@"bltcon0"]) {
			blitter.set_control(0, param1);
			continue;
		}
		if([type isEqualToString:@"bltcon1"]) {
			blitter.set_control(1, param1);
			continue;
		}

		if([type isEqualToString:@"bltsize"]) {
			blitter.set_size(param1);
			continue;
		}

		if([type isEqualToString:@"bltafwm"]) {
			blitter.set_first_word_mask(param1);
			continue;
		}
		if([type isEqualToString:@"bltalwm"]) {
			blitter.set_last_word_mask(param1);
			continue;
		}

		if([type isEqualToString:@"bltadat"]) {
			blitter.set_data(0, param1);
			continue;
		}
		if([type isEqualToString:@"bltbdat"]) {
			blitter.set_data(1, param1);
			continue;
		}
		if([type isEqualToString:@"bltcdat"]) {
			blitter.set_data(2, param1);
			continue;
		}

		if([type isEqualToString:@"bltamod"]) {
			blitter.set_modulo<0>(param1);
			continue;
		}
		if([type isEqualToString:@"bltbmod"]) {
			blitter.set_modulo<1>(param1);
			continue;
		}
		if([type isEqualToString:@"bltcmod"]) {
			blitter.set_modulo<2>(param1);
			continue;
		}
		if([type isEqualToString:@"bltdmod"]) {
			blitter.set_modulo<3>(param1);
			continue;
		}

		if([type isEqualToString:@"bltaptl"]) {
			blitter.set_pointer<0, 0>(param1);
			continue;
		}
		if([type isEqualToString:@"bltbptl"]) {
			blitter.set_pointer<1, 0>(param1);
			continue;
		}
		if([type isEqualToString:@"bltcptl"]) {
			blitter.set_pointer<2, 0>(param1);
			continue;
		}
		if([type isEqualToString:@"bltdptl"]) {
			blitter.set_pointer<3, 0>(param1);
			continue;
		}

		if([type isEqualToString:@"bltapth"]) {
			blitter.set_pointer<0, 16>(param1);
			continue;
		}
		if([type isEqualToString:@"bltbpth"]) {
			blitter.set_pointer<1, 16>(param1);
			continue;
		}
		if([type isEqualToString:@"bltcpth"]) {
			blitter.set_pointer<2, 16>(param1);
			continue;
		}
		if([type isEqualToString:@"bltdpth"]) {
			blitter.set_pointer<3, 16>(param1);
			continue;
		}

		NSLog(@"Unhandled type: %@", type);
		XCTAssert(false);
		break;
	}

	// Check the final set of writes.
	if(!hasFailed) {
		[self verifyWrites:writes blitter:blitter ram:ram approximateLocation:-1];
	}
}

- (void)testGadgetToggle {
	[self testCase:@"gadget toggle"];
}

- (void)testIconHighlight {
	[self testCase:@"icon highlight"];
}

- (void)testKickstart13BootLogo {
	[self testCase:@"kickstart13 boot logo"];
}

- (void)testSectorDecode {
	[self testCase:@"sector decode"];
}

- (void)testWindowDrag {
	[self testCase:@"window drag"];
}

- (void)testWindowResize {
	[self testCase:@"window resize"];
}

- (void)testRAMDiskOpen {
	[self testCase:@"RAM disk open"];
}

- (void)testSpots {
	[self testCase:@"spots"];
}

- (void)testClock {
	[self testCase:@"clock"];
}

- (void)testInclusiveFills {
	[self testCase:@"inclusive fills"];
}

- (void)testSequencer {
	// These patterns are faithfully transcribed from the HRM's
	// 'Pipeline Register' section, as captured online at
	// http://www.amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0127.html
	NSArray<NSString *> *const patterns = @[
		/* 0 */ @"- - - -",
		/* 1 */ @"D0 - D1 - D2",
		/* 2 */ @"C0 - C1 - C2",
		/* 3 */ @"C0 - - C1 D0 - C2 D1 - D2",
		/* 4 */ @"B0 - - B1 - - B2",
		/* 5 */ @"B0 - - B1 D0 - B2 D1 - D2",
		/* 6 */ @"B0 C0 - B1 C1 - B2 C2",
		/* 7 */ @"B0 C0 - - B1 C1 D0 - B2 C2 D1 - D2",
		/* 8 */ @"A0 - A1 - A2",
		/* 9 */ @"A0 - A1 D0 A2 D1 - D2",
		/* A */ @"A0 C0 A1 C1 A2 C2",
		/* B */ @"A0 C0 - A1 C1 D0 A2 C2 D1 - D2",
		/* C */ @"A0 B0 - A1 B1 - A2 B2",
		/* D */ @"A0 B0 - A1 B1 D0 A2 B2 D1 - D2",
		/* E */ @"A0 B0 C0 A1 B1 C1 A2 B2 C2",
		/* F */ @"A0 B0 C0 - A1 B1 C1 D0 A2 B2 C2 D1 D2",
	];

	for(int c = 0; c < 16; c++) {
		Amiga::BlitterSequencer sequencer;
		sequencer.set_control(c);

		int counts[4]{};
		const int writes = 2;
		NSUInteger length = [[patterns[c] componentsSeparatedByString:@" "] count];
		bool is_first_write = c > 1;	// control = 1 is D only, in which case don't pipeline.
		NSMutableArray<NSString *> *const components = [[NSMutableArray alloc] init];

		while(length--) {
			const auto next = sequencer.next();

			using Channel = Amiga::BlitterSequencer::Channel;
			switch(next) {
				case Channel::None:	[components addObject:@"-"];	break;
				case Channel::A:	[components addObject:[NSString stringWithFormat:@"A%d", counts[0]++]]; break;
				case Channel::B:	[components addObject:[NSString stringWithFormat:@"B%d", counts[1]++]]; break;
				case Channel::C:	[components addObject:[NSString stringWithFormat:@"C%d", counts[2]++]]; break;

				case Channel::Write:
					if(is_first_write) {
						is_first_write = false;
						[components addObject:@"-"];
					} else {
						[components addObject:[NSString stringWithFormat:@"D%d", counts[3]++]];
						if(counts[3] == writes) sequencer.complete();
					}
				break;

				default: break;
			}
		}

		NSString *pattern = [components componentsJoinedByString:@" "];
		XCTAssertEqualObjects(
			pattern,
			patterns[c],
			@"Pattern didn't match for control value %x", c);
	}
}

@end
