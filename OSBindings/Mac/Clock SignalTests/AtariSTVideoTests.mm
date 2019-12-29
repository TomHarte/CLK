//
//  MasterSystemVDPTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 09/10/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <memory>

#include "../../../Machines/Atari/ST/Video.hpp"

@interface AtariSTVideoTests : XCTestCase
@end

@implementation AtariSTVideoTests {
	std::unique_ptr<Atari::ST::Video> _video;
	uint16_t _ram[256*1024];
}

// MARK: - Setup and tear down.

- (void)setUp {
	[super setUp];

	// Establish an instance of video.
	_video = std::make_unique<Atari::ST::Video>();
	_video->set_ram(_ram, sizeof(_ram));
}

- (void)tearDown {
	[super tearDown];

	// Release the video instance.
	_video = nullptr;
}

// MARK: - Helpers

- (void)runVideoForCycles:(int)cycles {
	while(cycles--) {
		_video->run_for(Cycles(1));
	}
}

- (void)syncToStartOfLine {
	// Run until the visible fetch address changes, to get to the start of the pixel zone.
	const uint32_t original_address = [self currentVideoAddress];
	while(original_address == [self currentVideoAddress]) {
		_video->run_for(Cycles(1));
	}

	// Run until start of hsync.
	while(!_video->hsync()) {
		_video->run_for(Cycles(1));
	}

	// Run until end of hsync.
	while(_video->hsync()) {
		_video->run_for(Cycles(1));
	}
}

- (void)setFrequency:(int)frequency {
	switch(frequency) {
		default:
		case 50:	_video->write(0x05, 0x200);	_video->write(0x30, 0x000);	break;
		case 60:	_video->write(0x05, 0x000); _video->write(0x30, 0x000);	break;
		case 72:								_video->write(0x30, 0x200);	break;
	}
}

- (uint32_t)currentVideoAddress {
	return
		(_video->read(0x04) & 0xff) |
		((_video->read(0x03) & 0xff) << 8) |
		((_video->read(0x02) & 0xff) << 16);
}

- (void)setVideoBaseAddress:(uint32_t)baseAddress {
	_video->write(0x00, baseAddress >> 16);
	_video->write(0x01, baseAddress >> 8);
}

// MARK: - Sequence Point Prediction Tests

/// Tests that no events occur outside of the sequence points the video predicts.
- (void)testSequencePoints {
	// Set 4bpp, 50Hz.
	_video->write(0x05, 0x0200);
	_video->write(0x30, 0x0000);

	// Run for [more than] a whole frame making sure that no observeable outputs
	// change at any time other than a sequence point.
	HalfCycles next_event;
	bool display_enable = false;
	bool vsync = false;
	bool hsync = false;
	for(size_t c = 0; c < 10 * 1000 * 1000; ++c) {
		const bool is_transition_point = next_event == HalfCycles(0);

		if(is_transition_point) {
			display_enable = _video->display_enabled();
			vsync = _video->vsync();
			hsync = _video->hsync();
			next_event = _video->get_next_sequence_point();
		} else {
			NSAssert(display_enable == _video->display_enabled(), @"Unannounced change in display enabled at cycle %zu [%d before next sequence point]", c, next_event.as<int>());
			NSAssert(vsync == _video->vsync(), @"Unannounced change in vsync at cycle %zu [%d before next sequence point]", c, next_event.as<int>());
			NSAssert(hsync == _video->hsync(), @"Unannounced change in hsync at cycle %zu [%d before next sequence point]", c, next_event.as<int>());
		}
		_video->run_for(HalfCycles(2));
		next_event -= HalfCycles(2);
	}
}

// MARK: - Sync Line Length Tests

struct RunLength {
	int frequency;
	int length;
};

- (void)testSequence:(const RunLength *)sequence targetLength:(int)duration {
	[self syncToStartOfLine];

	const uint32_t start_address = [self currentVideoAddress];

	while(sequence->frequency != -1) {
		[self setFrequency:sequence->frequency];
		[self runVideoForCycles:sequence->length];
		++sequence;
	}
	const uint32_t final_address = [self currentVideoAddress];

	XCTAssertEqual(final_address - start_address, duration);
}

- (void)testLineLength54 {
	// Run as though a regular 50Hz line at least until cycle 52;
	// then switch to 72 Hz by 164, and allow the line to finish.
	const RunLength test[] = {
		{50, 60},
		{72, 452},
		{-1}
	};
	[self testSequence:test targetLength:54];
}

- (void)testLineLength56 {
	// Run as though a regular 60Hz line at least until cycle 52;
	// then switch to 72 Hz by 164, and allow the line to finish.
	const RunLength test[] = {
		{60, 60},
		{72, 452},
		{-1}
	};
	[self testSequence:test targetLength:56];
}

- (void)testLineLength80 {
	// Run a standard 72Hz line.
	const RunLength test[] = {
		{72, 224},
		{-1}
	};
	[self testSequence:test targetLength:80];
}

- (void)testLineLengthLong80 {
	// Run a 72Hz line with a switch through 50Hz to extend the length to 512 cycles.
	const RunLength test[] = {
		{72, 50},
		{50, 20},
		{72, 442},
		{-1}
	};
	[self testSequence:test targetLength:80];
}

- (void)testLineLength158 {
	// Transition from 50Hz to 60Hz mid-line.
	const RunLength test[] = {
		{50, 60},
		{60, 458},
		{-1}
	};
	[self testSequence:test targetLength:158];
}

- (void)testLineLength160_60Hz {
	const RunLength test[] = {
		{60, 508},
		{-1}
	};
	[self testSequence:test targetLength:160];
}

- (void)testLineLength160_50Hz {
	const RunLength test[] = {
		{50, 512},
		{-1}
	};
	[self testSequence:test targetLength:160];
}

- (void)testLineLength162 {
	// Transition from 60Hz to 50Hz mid-line.
	const RunLength test[] = {
		{60, 54},
		{50, 458},
		{-1}
	};
	[self testSequence:test targetLength:162];
}

- (void)testLineLength184 {
	// Start off in 72Hz, switch to 60 during pixels.
	const RunLength test[] = {
		{72, 8},
		{60, 500},
		{-1}
	};
	[self testSequence:test targetLength:184];
}

- (void)testLineLength186 {
	// Start off in 72Hz, switch to 50 during pixels.
	const RunLength test[] = {
		{72, 8},
		{50, 504},
		{-1}
	};
	[self testSequence:test targetLength:186];
}

- (void)testLineLength204 {
	// Start in 50Hz, avoid DE disable.
	const RunLength test[] = {
		{50, 374},
		{60, 138},
		{-1}
	};
	[self testSequence:test targetLength:204];
}

- (void)testLineLength206 {
	// Start in 60Hz, get a 50Hz line length, avoid DE disable.
	const RunLength test[] = {
		{60, 53},
		{50, 3},		// To 56.
		{60, 314},		// 370.
		{50, 4},		// 374.
		{60, 138},		// 512.
		{-1}
	};
	[self testSequence:test targetLength:206];
}

- (void)testLineLength230 {
	// Start in 72Hz, avoid DE disable.
	const RunLength test[] = {
		{72, 8},
		{50, 366},
		{60, 138},
		{-1}
	};
	[self testSequence:test targetLength:230];
}

// MARK: - Address Reload Timing tests

/// Tests that the current video address is reloaded constantly throughout hsync
- (void)testHsyncReload {
	// Set an initial video address of 0.
	[self setVideoBaseAddress:0];

	// Find next area of non-vsync.
	while(_video->vsync()) {
		_video->run_for(Cycles(1));
	}

	// Set a different base video address.
	[self setVideoBaseAddress:0x800000];

	// Find next area of vsync, checking that the address isn't
	// reloaded before then.
	while(!_video->vsync()) {
		XCTAssertNotEqual([self currentVideoAddress], 0x800000);
		_video->run_for(Cycles(1));
	}

	// Vsync has now started, test that video address has been set.
	XCTAssertEqual([self currentVideoAddress], 0x800000);

	// Run a few cycles, set a different video base address,
	// confirm that has been set.
	[self runVideoForCycles:200];
	XCTAssertEqual([self currentVideoAddress], 0x800000);
	[self setVideoBaseAddress:0xc00000];
	[self runVideoForCycles:1];
	XCTAssertEqual([self currentVideoAddress], 0xc00000);

	// Find end of vertical sync, set a different base address,
	// check that it doesn't become current.
	while(_video->vsync()) {
		_video->run_for(Cycles(1));
	}
	[self setVideoBaseAddress:0];
	[self runVideoForCycles:1];
	XCTAssertNotEqual([self currentVideoAddress], 0);
}

// MARK: - Tests Correlating To Exact Pieces of Software

- (void)testUnionDemoScroller {
	const RunLength test[] = {
		{72, 8},
		{50, 365},
		{60, 8},
		{50, 59},
		{72, 12},
		{50, 60},
		{-1}
	};
	[self testSequence:test targetLength:230];
}

@end
