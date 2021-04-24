//
//  SpectrumVideoContentionTests.cpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 23/4/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Machines/Sinclair/ZXSpectrum/Video.hpp"

@interface SpectrumVideoContentionTests : XCTestCase
@end

@implementation SpectrumVideoContentionTests

struct ContentionAnalysis {
	int time_after_interrupt = 0;
	int contended_lines = 0;
	int contention_length = 0;
	int line_length = 0;
	int total_lines = 0;
	HalfCycles pattern[8];
};

template <Sinclair::ZXSpectrum::VideoTiming video_timing> ContentionAnalysis analyse() {
	Sinclair::ZXSpectrum::Video<video_timing> video;
	ContentionAnalysis analysis;

	// Advance to the start of the first interrupt.
	while(video.get_interrupt_line()) {
		video.run_for(HalfCycles(1));
	}
	while(!video.get_interrupt_line()) {
		video.run_for(HalfCycles(1));
	}

	// Count half cycles until first non-zero contended time.
	while(video.access_delay(HalfCycles(0)) == HalfCycles(0)) {
		video.run_for(HalfCycles(2));
		analysis.time_after_interrupt += 2;
	}

	// Grab the contention pattern.
	for(int c = 0; c < 8; c++) {
		analysis.pattern[c] = video.access_delay(HalfCycles(0));
		video.run_for(HalfCycles(2));
	}

	// Figure out how long contention goes on for.
	int c = 0;
	analysis.contention_length = 16;	// For the 16 just skipped.
	do {
		analysis.contention_length += 2;
		video.run_for(HalfCycles(2));
		c++;
	} while(analysis.pattern[c&7] == video.access_delay(HalfCycles(0)));

	// Look for next start of contention to determine line length.
	analysis.line_length = analysis.contention_length;
	while(video.access_delay(HalfCycles(0)) == HalfCycles(0)) {
		video.run_for(HalfCycles(2));
		analysis.line_length += 2;
	}

	// Count contended lines.
	analysis.contended_lines = 1;
	while(video.access_delay(HalfCycles(0)) == analysis.pattern[0]) {
		video.run_for(HalfCycles(analysis.line_length));
		++analysis.contended_lines;
	}

	// Count total lines.
	analysis.total_lines = analysis.contended_lines;
	while(video.access_delay(HalfCycles(0)) != analysis.pattern[0]) {
		video.run_for(HalfCycles(analysis.line_length));
		++analysis.total_lines;
	}

	return analysis;
}

- (void)test48k {
	const auto analysis = analyse<Sinclair::ZXSpectrum::VideoTiming::FortyEightK>();

	// Check time from interrupt.
	XCTAssertEqual(analysis.time_after_interrupt, 14335*2);

	// Check contention pattern.
	XCTAssertEqual(analysis.pattern[0], HalfCycles(6 * 2));
	XCTAssertEqual(analysis.pattern[1], HalfCycles(5 * 2));
	XCTAssertEqual(analysis.pattern[2], HalfCycles(4 * 2));
	XCTAssertEqual(analysis.pattern[3], HalfCycles(3 * 2));
	XCTAssertEqual(analysis.pattern[4], HalfCycles(2 * 2));
	XCTAssertEqual(analysis.pattern[5], HalfCycles(1 * 2));
	XCTAssertEqual(analysis.pattern[6], HalfCycles(0 * 2));
	XCTAssertEqual(analysis.pattern[7], HalfCycles(0 * 2));

	// Check line length and count.
	XCTAssertEqual(analysis.contention_length, 128*2);
	XCTAssertEqual(analysis.line_length, 224*2);
	XCTAssertEqual(analysis.contended_lines, 192);
	XCTAssertEqual(analysis.total_lines, 312);
}

- (void)test128k {
	const auto analysis = analyse<Sinclair::ZXSpectrum::VideoTiming::OneTwoEightK>();

	// Check time from interrupt.
	XCTAssertEqual(analysis.time_after_interrupt, 14361*2);

	// Check contention pattern.
	XCTAssertEqual(analysis.pattern[0], HalfCycles(6 * 2));
	XCTAssertEqual(analysis.pattern[1], HalfCycles(5 * 2));
	XCTAssertEqual(analysis.pattern[2], HalfCycles(4 * 2));
	XCTAssertEqual(analysis.pattern[3], HalfCycles(3 * 2));
	XCTAssertEqual(analysis.pattern[4], HalfCycles(2 * 2));
	XCTAssertEqual(analysis.pattern[5], HalfCycles(1 * 2));
	XCTAssertEqual(analysis.pattern[6], HalfCycles(0 * 2));
	XCTAssertEqual(analysis.pattern[7], HalfCycles(0 * 2));

	// Check line length and count.
	XCTAssertEqual(analysis.contention_length, 128*2);
	XCTAssertEqual(analysis.line_length, 228*2);
	XCTAssertEqual(analysis.contended_lines, 192);
	XCTAssertEqual(analysis.total_lines, 311);
}

- (void)testPlus3 {
	const auto analysis = analyse<Sinclair::ZXSpectrum::VideoTiming::Plus3>();

	// Check time from interrupt.
	XCTAssertEqual(analysis.time_after_interrupt, 14361*2);

	// Check contention pattern.
	XCTAssertEqual(analysis.pattern[0], HalfCycles(1 * 2));
	XCTAssertEqual(analysis.pattern[1], HalfCycles(0 * 2));
	XCTAssertEqual(analysis.pattern[2], HalfCycles(7 * 2));
	XCTAssertEqual(analysis.pattern[3], HalfCycles(6 * 2));
	XCTAssertEqual(analysis.pattern[4], HalfCycles(5 * 2));
	XCTAssertEqual(analysis.pattern[5], HalfCycles(4 * 2));
	XCTAssertEqual(analysis.pattern[6], HalfCycles(3 * 2));
	XCTAssertEqual(analysis.pattern[7], HalfCycles(2 * 2));

	// Check line length and count.
	XCTAssertEqual(analysis.contention_length, 130*2);	// By the manner used for detection above,
														// the first obviously missing contention spot
														// will be after 130 cycles, not the textbook 129,
														// because cycle 130 is a delay of 0 either way.
	XCTAssertEqual(analysis.line_length, 228*2);
	XCTAssertEqual(analysis.contended_lines, 192);
	XCTAssertEqual(analysis.total_lines, 311);
}

@end
