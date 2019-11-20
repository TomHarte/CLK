//
//  MasterSystemVDPTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 09/10/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Machines/Atari/ST/Video.hpp"

@interface AtariSTVideoTests : XCTestCase
@end

@implementation AtariSTVideoTests

- (void)setUp {
	[super setUp];
}

- (void)tearDown {
	[super tearDown];
}

- (void)testSequencePoints {
	// Establish an instance of video.
	Atari::ST::Video video;
	uint16_t ram[256*1024];
	video.set_ram(ram, sizeof(ram));

	// Set 4bpp, 50Hz.
	video.write(0x05, 0x0200);
	video.write(0x30, 0x0000);

	// Run for [more than] a whole frame making sure that no observeable outputs
	// change at any time other than a sequence point.
	HalfCycles next_event;
	bool display_enable = false;
	bool vsync = false;
	bool hsync = false;
	for(size_t c = 0; c < 10 * 1000 * 1000; ++c) {
		const bool is_transition_point = next_event == HalfCycles(0);

		if(is_transition_point) {
			display_enable = video.display_enabled();
			vsync = video.vsync();
			hsync = video.hsync();
			next_event = video.get_next_sequence_point();
		} else {
			NSAssert(display_enable == video.display_enabled(), @"Unannounced change in display enabled at cycle %zu [%d before next sequence point]", c, next_event.as<int>());
			NSAssert(vsync == video.vsync(), @"Unannounced change in vsync at cycle %zu [%d before next sequence point]", c, next_event.as<int>());
			NSAssert(hsync == video.hsync(), @"Unannounced change in hsync at cycle %zu [%d before next sequence point]", c, next_event.as<int>());
		}
		video.run_for(HalfCycles(2));
		next_event -= HalfCycles(2);
	}
}

@end
