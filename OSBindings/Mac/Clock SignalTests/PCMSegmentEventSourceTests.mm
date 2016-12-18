//
//  PCMSegmentEventSourceTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "PCMSegment.hpp"

@interface PCMSegmentEventSourceTests : XCTestCase
@end

@implementation PCMSegmentEventSourceTests

- (Storage::Disk::PCMSegmentEventSource)segmentSource
{
	Storage::Disk::PCMSegment alternatingFFs;
	alternatingFFs.data = {0xff, 0x00, 0xff, 0x00};
	alternatingFFs.length_of_a_bit.length = 1;
	alternatingFFs.length_of_a_bit.clock_rate = 10;
	alternatingFFs.number_of_bits = 32;
	return Storage::Disk::PCMSegmentEventSource(alternatingFFs);
}

- (void)testCentring
{
	Storage::Disk::PCMSegmentEventSource segmentSource = self.segmentSource;
	[self assertFirstTwoEventLengthsForSource:segmentSource];
}

- (void)assertFirstTwoEventLengthsForSource:(Storage::Disk::PCMSegmentEventSource &)segmentSource
{
	Storage::Disk::Track::Event first_event = segmentSource.get_next_event();
	Storage::Disk::Track::Event second_event = segmentSource.get_next_event();

	first_event.length.simplify();
	second_event.length.simplify();
	XCTAssertTrue(first_event.length.length == 1 && first_event.length.clock_rate == 20, @"First event should occur half a bit's length in");
	XCTAssertTrue(second_event.length.length == 1 && second_event.length.clock_rate == 10, @"Second event should occur a whole bit's length after the first");
}

- (void)testLongerGap
{
	Storage::Disk::PCMSegmentEventSource segmentSource = self.segmentSource;

	// skip first eight flux transitions
	for(int c = 0; c < 8; c++) segmentSource.get_next_event();

	Storage::Disk::Track::Event next_event = segmentSource.get_next_event();
	next_event.length.simplify();

	XCTAssertTrue(next_event.length.length == 9 && next_event.length.clock_rate == 10, @"Zero byte should give a nine bit length event gap");
}

- (void)testTermination
{
	Storage::Disk::PCMSegmentEventSource segmentSource = self.segmentSource;
	Storage::Time total_time;
	for(int c = 0; c < 16; c++) total_time += segmentSource.get_next_event().length;

	Storage::Disk::Track::Event final_event = segmentSource.get_next_event();
	total_time += final_event.length;
	total_time.simplify();

	XCTAssertTrue(final_event.type == Storage::Disk::Track::Event::IndexHole, @"Segment should end with an index hole");
	XCTAssertTrue(total_time.length == 16 && total_time.clock_rate == 5, @"Should have taken 32 bit lengths to finish the segment");
}

- (void)testReset
{
	Storage::Disk::PCMSegmentEventSource segmentSource = self.segmentSource;
	for(int c = 0; c < 8; c++) segmentSource.get_next_event();
	segmentSource.reset();
	[self assertFirstTwoEventLengthsForSource:segmentSource];
}

@end
