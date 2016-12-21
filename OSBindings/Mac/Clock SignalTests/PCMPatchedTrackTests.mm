//
//  PCMPatchedTrackTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "PCMTrack.hpp"
#include "PCMPatchedTrack.hpp"

@interface PCMPatchedTrackTests : XCTestCase
@end

@implementation PCMPatchedTrackTests

- (std::shared_ptr<Storage::Disk::Track>)togglingTrack
{
	Storage::Disk::PCMSegment segment;
	segment.data = { 0xff, 0xff, 0xff, 0xff };
	segment.number_of_bits = 32;
	return std::shared_ptr<Storage::Disk::Track>(new Storage::Disk::PCMTrack(segment));
}

- (std::shared_ptr<Storage::Disk::Track>)patchableTogglingTrack
{
	std::shared_ptr<Storage::Disk::Track> track = self.togglingTrack;
	return std::shared_ptr<Storage::Disk::Track>(new Storage::Disk::PCMPatchedTrack(track));
}

- (void)assertOneThirtyTwosForTrack:(std::shared_ptr<Storage::Disk::Track>)track
{
	// Confirm that there are now flux transitions (just the first five will do)
	// located 1/32nd of a rotation apart.
	for(int c = 0; c < 5; c++)
	{
		Storage::Disk::Track::Event event = track->get_next_event();
		XCTAssert(
			event.length == (c ? Storage::Time(1, 32) : Storage::Time(1, 64)),
			@"flux transitions should be 1/32nd of a track apart");
	}
}

- (void)testUnpatchedRawTrack
{
	[self assertOneThirtyTwosForTrack:self.togglingTrack];
}

- (void)testUnpatchedTrack
{
	[self assertOneThirtyTwosForTrack:self.patchableTogglingTrack];
}

- (void)testSingleSplice
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	Storage::Disk::PCMPatchedTrack *patchable = static_cast<Storage::Disk::PCMPatchedTrack *>(patchableTrack.get());

	// add a single one, at 1/32 length at 3/128. So that should shift the location of the second flux transition
	Storage::Disk::PCMSegment one_segment;
	one_segment.data = {0xff};
	one_segment.number_of_bits = 1;
	one_segment.length_of_a_bit.length = 1;
	one_segment.length_of_a_bit.clock_rate = 32;
	patchable->add_segment(Storage::Time(3, 128), one_segment);

	Storage::Time total_length;
	std::vector<Storage::Disk::Track::Event> events;
	while(1)
	{
		events.push_back(patchableTrack->get_next_event());
		total_length += events.back().length;
		if(events.back().type == Storage::Disk::Track::Event::IndexHole) break;
	}

	XCTAssert(events.size() == 33, @"Should still be 33 total events");
	XCTAssert(events[0].length == Storage::Time(1, 64), @"First event should be after 1/64 as usual");
	XCTAssert(events[1].length == Storage::Time(3, 128), @"Second event should be 3/128 later");	// ... as it was inserted at 3/128 and runs at the same rate as the main data, so first inserted event is at 3/128+1/64-1/64
	XCTAssert(events[2].length == Storage::Time(5, 128), @"Should still be 33 total events");	// 1/64 = 2/128 to exit the patch, plus 3/128 to get to the next event, having spliced in 1/128 ahead of the normal clock
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testLeftReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	Storage::Disk::PCMPatchedTrack *patchable = static_cast<Storage::Disk::PCMPatchedTrack *>(patchableTrack.get());

	Storage::Disk::PCMSegment zero_segment;
	zero_segment.data = {0x00};
	zero_segment.number_of_bits = 8;
	zero_segment.length_of_a_bit.length = 1;
	zero_segment.length_of_a_bit.clock_rate = 16;
	patchable->add_segment(Storage::Time(0), zero_segment);

	Storage::Time total_length;
	std::vector<Storage::Disk::Track::Event> events;
	while(1)
	{
		events.push_back(patchableTrack->get_next_event());
		total_length += events.back().length;
		if(events.back().type == Storage::Disk::Track::Event::IndexHole) break;
	}

	XCTAssert(events.size() == 17, @"Should still be 17 total events");
	XCTAssert(events[0].length == Storage::Time(33, 64), @"First event should not occur until 33/64");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testRightReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	Storage::Disk::PCMPatchedTrack *patchable = static_cast<Storage::Disk::PCMPatchedTrack *>(patchableTrack.get());

	Storage::Disk::PCMSegment zero_segment;
	zero_segment.data = {0x00};
	zero_segment.number_of_bits = 8;
	zero_segment.length_of_a_bit.length = 1;
	zero_segment.length_of_a_bit.clock_rate = 16;
	patchable->add_segment(Storage::Time(1, 2), zero_segment);

	Storage::Time total_length;
	std::vector<Storage::Disk::Track::Event> events;
	while(1)
	{
		events.push_back(patchableTrack->get_next_event());
		total_length += events.back().length;
		if(events.back().type == Storage::Disk::Track::Event::IndexHole) break;
	}

	XCTAssert(events.size() == 17, @"Should still be 17 total events");
	XCTAssert(events[16].length == Storage::Time(33, 64), @"Final event should take 33/64");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (std::shared_ptr<Storage::Disk::Track>)fourSegmentPatchedTrack
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	Storage::Disk::PCMPatchedTrack *patchable = static_cast<Storage::Disk::PCMPatchedTrack *>(patchableTrack.get());

	for(int c = 0; c < 4; c++)
	{
		Storage::Disk::PCMSegment segment;
		segment.data = {0xff};
		segment.number_of_bits = 8;
		segment.length_of_a_bit.length = 1;
		segment.length_of_a_bit.clock_rate = 32;
		patchable->add_segment(Storage::Time(c, 4), segment);
	}

	return patchableTrack;
}

- (void)testMultiSegmentTrack
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;

	Storage::Time total_length;
	std::vector<Storage::Disk::Track::Event> events;
	while(1)
	{
		events.push_back(patchableTrack->get_next_event());
		total_length += events.back().length;
		if(events.back().type == Storage::Disk::Track::Event::IndexHole) break;
	}

	XCTAssert(events.size() == 33, @"Should still be 33 total events");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testMultiReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;
	Storage::Disk::PCMPatchedTrack *patchable = static_cast<Storage::Disk::PCMPatchedTrack *>(patchableTrack.get());

	Storage::Disk::PCMSegment segment;
	segment.data = {0x00};
	segment.number_of_bits = 8;
	segment.length_of_a_bit.length = 1;
	segment.length_of_a_bit.clock_rate = 16;
	patchable->add_segment(Storage::Time(1, 8), segment);

	Storage::Time total_length;
	std::vector<Storage::Disk::Track::Event> events;
	while(1)
	{
		events.push_back(patchableTrack->get_next_event());
		total_length += events.back().length;
		if(events.back().type == Storage::Disk::Track::Event::IndexHole) break;
	}

	XCTAssert(events.size() == 17, @"Should still be 17 total events");
	XCTAssert(events[4].length == Storage::Time(17, 32), @"Should have added a 17/32 gap after the fourth event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

@end
