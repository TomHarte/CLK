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
	int c = 5;
	while(c--)
	{
		Storage::Disk::Track::Event event = track->get_next_event();
		Storage::Time simplified_time = event.length.simplify();
		XCTAssert(simplified_time.length == 1 && simplified_time.clock_rate == 32, "flux transitions should be 1/32nd of a track apart");
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

- (void)testZeroPatch
{
	std::shared_ptr<Storage::Disk::Track> patchableTrack = self.patchableTogglingTrack;
	Storage::Disk::PCMPatchedTrack *patchable = dynamic_cast<Storage::Disk::PCMPatchedTrack *>(patchableTrack.get());
	if(patchable)
	{
		// add a single one, at 1/32 length at 3/128. So that should shift the location of the second flux transition
		Storage::Disk::PCMSegment zero_segment;
		zero_segment.data = {0xff};
		zero_segment.number_of_bits = 1;
		zero_segment.length_of_a_bit.length = 1;
		zero_segment.length_of_a_bit.clock_rate = 32;
		patchable->add_segment(Storage::Time(3, 128), zero_segment);
	}

	std::vector<Storage::Disk::Track::Event> events;
	int c = 5;
	while(c--)
	{
		events.push_back(patchableTrack->get_next_event());
	}
}

@end
