//
//  PCMTrackTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 18/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "PCMTrack.hpp"

@interface PCMTrackTests : XCTestCase
@end

@implementation PCMTrackTests

- (Storage::Disk::PCMTrack)multiSpeedTrack
{
	Storage::Disk::PCMSegment quickSegment, slowSegment;

	quickSegment.data = {0xff};
	quickSegment.number_of_bits = 8;
	quickSegment.length_of_a_bit.length = 1;
	quickSegment.length_of_a_bit.clock_rate = 100;

	slowSegment.data = {0xff};
	slowSegment.number_of_bits = 8;
	slowSegment.length_of_a_bit.length = 1;
	slowSegment.length_of_a_bit.clock_rate = 3;

	return Storage::Disk::PCMTrack({quickSegment, slowSegment});
}

- (void)testMultispeedTrack
{
	Storage::Disk::PCMTrack track = self.multiSpeedTrack;
	std::vector<Storage::Disk::Track::Event> events;
	Storage::Time total_length;
	do {
		events.push_back(track.get_next_event());
		total_length += events.back().length;
	} while(events.back().type != Storage::Disk::Track::Event::IndexHole);

	XCTAssert(events.size() == 17, "Should have received 17 events; got %lu", events.size());

	total_length.simplify();
	XCTAssert(total_length.length == 1 && total_length.clock_rate == 1, "Events should have summed to a total time of 1; instead got %u/%u", total_length.length, total_length.clock_rate);

	Storage::Time transition_length = events[0].length + events.back().length;
	XCTAssert(events[8].length == transition_length, "Time taken in transition between speed zones should be half of a bit length in the first part plus half of a bit length in the second");
}

- (void)testComplicatedTrackSeek {
	std::vector<Storage::Disk::PCMSegment> segments;

	Storage::Disk::PCMSegment sync_segment;
	sync_segment.data.resize(10);
	sync_segment.number_of_bits = 10*8;

	Storage::Disk::PCMSegment header_segment;
	header_segment.data.resize(14);
	header_segment.number_of_bits = 14*8;

	Storage::Disk::PCMSegment data_segment;
	data_segment.data.resize(349);
	data_segment.number_of_bits = 349*8;

	for(std::size_t c = 0; c < 16; ++c) {
		segments.push_back(sync_segment);
		segments.push_back(header_segment);
		segments.push_back(sync_segment);
		segments.push_back(data_segment);
		segments.push_back(sync_segment);
	}

	Storage::Disk::PCMTrack track(segments);
	Storage::Time late_time(967445, 2045454);
	track.seek_to(late_time);
	track.get_next_event();
}

@end
