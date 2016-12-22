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

#pragma mark - Prebuilt tracks

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

#pragma mark -

- (std::vector<Storage::Disk::Track::Event>)eventsFromTrack:(std::shared_ptr<Storage::Disk::Track>)track
{
	std::vector<Storage::Disk::Track::Event> events;
	while(1)
	{
		events.push_back(track->get_next_event());
		if(events.back().type == Storage::Disk::Track::Event::IndexHole) break;
	}
	return events;
}

- (Storage::Time)timeForEvents:(const std::vector<Storage::Disk::Track::Event> &)events
{
	Storage::Time result(0);
	for(auto event : events)
	{
		result += event.length;
	}
	return result;
}

- (void)patchTrack:(std::shared_ptr<Storage::Disk::Track>)track withSegment:(Storage::Disk::PCMSegment)segment atTime:(Storage::Time)time
{
	Storage::Disk::PCMPatchedTrack *patchable = static_cast<Storage::Disk::PCMPatchedTrack *>(track.get());
	patchable->add_segment(time, segment);
}

#pragma mark - Repeating Asserts

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

#pragma mark - Unpatched tracks

- (void)testUnpatchedRawTrack
{
	[self assertOneThirtyTwosForTrack:self.togglingTrack];
}

- (void)testUnpatchedTrack
{
	[self assertOneThirtyTwosForTrack:self.patchableTogglingTrack];
}

#pragma mark - Insertions affecting one existing segment

- (void)testSingleSplice
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(1, 32), 1, {0xff}) atTime:Storage::Time(3, 128)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 33, @"Should still be 33 total events");
	XCTAssert(events[0].length == Storage::Time(1, 64), @"First event should be after 1/64 as usual");
	XCTAssert(events[1].length == Storage::Time(3, 128), @"Second event should be 3/128 later");	// ... as it was inserted at 3/128 and runs at the same rate as the main data, so first inserted event is at 3/128+1/64-1/64
	XCTAssert(events[2].length == Storage::Time(5, 128), @"Should still be 33 total events");	// 1/64 = 2/128 to exit the patch, plus 3/128 to get to the next event, having spliced in 1/128 ahead of the normal clock
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testLeftReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(1, 16), 8, {0x00}) atTime:Storage::Time(0)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 17, @"Should still be 17 total events");
	XCTAssert(events[0].length == Storage::Time(33, 64), @"First event should not occur until 33/64");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testRightReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(1, 16), 8, {0x00}) atTime:Storage::Time(1, 2)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 17, @"Should still be 17 total events");
	XCTAssert(events[16].length == Storage::Time(33, 64), @"Final event should take 33/64");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

#pragma mark - Insertions affecting three existing segments

- (void)testMultiSegmentTrack
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 33, @"Should still be 33 total events");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testMultiTrimBothSideReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(1, 16), 8, {0x00}) atTime:Storage::Time(1, 8)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 17, @"Should still be 17 total events");
	XCTAssert(events[4].length == Storage::Time(17, 32), @"Should have added a 17/32 gap after the fourth event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testMultiTrimRightReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(3, 8), 1, {0x00}) atTime:Storage::Time(1, 8)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 21, @"Should still be 17 total events");
	XCTAssert(events[4].length == Storage::Time(13, 32), @"Should have added a 13/32 gap after the fourth event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testMultiTrimLeftReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(3, 8), 1, {0x00}) atTime:Storage::Time(1, 4)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 21, @"Should still be 17 total events");
	XCTAssert(events[8].length == Storage::Time(13, 32), @"Should have added a 13/32 gap after the eighth event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

#pragma mark - Insertions affecting two existing segments

- (void)testTwoSegmentOverlap
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(1, 32), 8, {0x00}) atTime:Storage::Time(1, 8)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 25, @"Should still be 25 total events");
	XCTAssert(events[4].length == Storage::Time(18, 64), @"Should have added an 18/64 gap after the fourth event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testTwoSegmentRightReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(3, 8), 1, {0x00}) atTime:Storage::Time(1, 8)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 21, @"Should still be 25 total events");
	XCTAssert(events[4].length == Storage::Time(13, 32), @"Should have added an 13/32 gap after the fourth event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

- (void)testTwoSegmentLeftReplace
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.fourSegmentPatchedTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(3, 8), 1, {0x00}) atTime:Storage::Time(0)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 21, @"Should still be 25 total events");
	XCTAssert(events[0].length == Storage::Time(25, 64), @"Should have added an 25/64 gap after the fourth event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

#pragma mark - Wrapping segment

- (void)testWrappingSegment
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	[self patchTrack:patchableTrack withSegment:Storage::Disk::PCMSegment(Storage::Time(5, 2), 1, {0x00}) atTime:Storage::Time(0)];

	std::vector<Storage::Disk::Track::Event> events = [self eventsFromTrack:patchableTrack];
	Storage::Time total_length = [self timeForEvents:events];

	XCTAssert(events.size() == 1, @"Should be only one event");
	XCTAssert(total_length == Storage::Time(1), @"Total track length should still be 1");
}

@end
