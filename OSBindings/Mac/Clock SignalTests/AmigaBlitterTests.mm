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

@interface AmigaBlitterTests: XCTestCase
@end

@implementation AmigaBlitterTests

- (void)testCase:(NSString *)name {
	uint16_t ram[256 * 1024]{};
	Amiga::Chipset nonChipset;
	Amiga::Blitter<true> blitter(nonChipset, ram, 256 * 1024);

	NSURL *const traceURL = [[NSBundle bundleForClass:[self class]] URLForResource:name withExtension:@"json" subdirectory:@"Amiga Blitter Tests"];
	NSData *const traceData = [NSData dataWithContentsOfURL:traceURL];
	NSArray *const trace = [NSJSONSerialization JSONObjectWithData:traceData options:0 error:nil];

	NSUInteger index = -1;
	for(NSArray *const event in trace) {
		NSString *const type = event[0];
		const NSInteger param1 = [event[1] integerValue];
		++index;

		//
		// Register writes. Pass straight along.
		//
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

		//
		// Bus activity. Store as initial state, and translate for comparison.
		//
		Amiga::Blitter<true>::Transaction expected_transaction;
		using TransactionType = Amiga::Blitter<true>::Transaction::Type;
		expected_transaction.address = uint32_t(param1 >> 1);
		expected_transaction.value = uint16_t([event[2] integerValue]);

		if([type isEqualToString:@"cread"] || [type isEqualToString:@"bread"] || [type isEqualToString:@"aread"]) {
			XCTAssert(param1 < sizeof(ram) - 1);
			ram[param1 >> 1] = [event[2] integerValue];

			if([type isEqualToString:@"aread"]) expected_transaction.type = TransactionType::ReadA;
			if([type isEqualToString:@"bread"]) expected_transaction.type = TransactionType::ReadB;
			if([type isEqualToString:@"cread"]) expected_transaction.type = TransactionType::ReadC;
		} else if([type isEqualToString:@"write"]) {
			expected_transaction.type = TransactionType::WriteFromPipeline;
		} else {
			NSLog(@"Unhandled type: %@", type);
			XCTAssert(false);
			break;
		}

		// Loop until another [comparable] bus transaction appears, and test.
		while(true) {
			blitter.advance_dma();

			const auto transactions = blitter.get_and_reset_transactions();
			if(transactions.empty()) {
				continue;
			}

			bool did_compare = false;
			for(const auto &transaction : transactions) {
				// Skipped slots and data coming out of the pipeline aren't captured
				// by the original test data.
				switch(transaction.type) {
					case TransactionType::SkippedSlot:
					case TransactionType::WriteFromPipeline:
						continue;

					default: break;
				}

				XCTAssertEqual(transaction.type, expected_transaction.type, @"Type mismatch at index %lu", (unsigned long)index);
				XCTAssertEqual(transaction.value, expected_transaction.value, @"Value mismatch at index %lu", (unsigned long)index);
				XCTAssertEqual(transaction.address, expected_transaction.address, @"Address mismatch at index %lu", (unsigned long)index);
				if(
					transaction.type != expected_transaction.type ||
					transaction.value != expected_transaction.value ||
					transaction.address != expected_transaction.address) {
					return;
				}

				did_compare = true;
			}
			if(did_compare) break;
		}
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

	for(int c = 0; c < 16; c++)	{
		Amiga::BlitterSequencer sequencer;
		sequencer.set_control(c);
		sequencer.begin();

		int writes = 0;
		NSUInteger length = [[patterns[c] componentsSeparatedByString:@" "] count];
		bool is_first_write = c > 1;	// control = 1 is D only, in which case don't pipeline.
		NSMutableArray<NSString *> *const components = [[NSMutableArray alloc] init];

		while(length--) {
			const auto next = sequencer.next();

			using Channel = Amiga::BlitterSequencer::Channel;
			switch(next.first) {
				case Channel::None:	[components addObject:@"-"];	break;
				case Channel::A:	[components addObject:[NSString stringWithFormat:@"A%d", next.second]]; break;
				case Channel::B:	[components addObject:[NSString stringWithFormat:@"B%d", next.second]]; break;
				case Channel::C:	[components addObject:[NSString stringWithFormat:@"C%d", next.second]]; break;

				case Channel::Write:
				case Channel::FlushPipeline:
					if(is_first_write) {
						is_first_write = false;
						[components addObject:@"-"];
					} else {
						[components addObject:[NSString stringWithFormat:@"D%d", writes++]];
					}
				break;

				default: break;
			}

			if(next.second == 2) {
				sequencer.complete();
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
