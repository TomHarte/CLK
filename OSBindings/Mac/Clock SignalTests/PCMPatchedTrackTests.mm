//
//  PCMPatchedTrackTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "PCMPatchedTrackTests.h"

#include "PCMTrack.hpp"
#include "PCMPatchedTrack.hpp"

@implementation PCMPatchedTrackTests

- (Storage::Disk::PCMTrack)togglingTrack
{
	Storage::Disk::PCMSegment segment;
	segment.data = { 0xff, 0xff, 0xff, 0xff };
	segment.number_of_bits = 32;
	return Storage::Disk::PCMTrack(segment);
}

- (void)testUnpatchedTrack
{
	Storage::Disk::PCMTrack track = self.togglingTrack;

	// Confirm that there are now flux transitions (just the first five will do)
	// located 1/32nd of a rotation apart.
	int c = 5;
	while(c--)
	{
		Storage::Disk::Track::Event event = track.get_next_event();
		Storage::Time simplified_time = event.length.simplify();
		XCTAssert(simplified_time.length == 1 && simplified_time.clock_rate == 32, "flux transitions should be 1/32nd of a track apart");
	}
}

@end
