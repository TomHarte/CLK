//
//  PCMTrackTests.m
//  Clock Signal
//
//  Created by Thomas Harte on 18/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "PCMTrack.hpp"

@interface PCMTrackTests : XCTestCase
@end

@implementation PCMTrackTests

- (Storage::Disk::PCMTrack)multiSpeedTrack
{
	Storage::Disk::PCMSegment quickSegment, slowSegment;

	quickSegment.data = {true, true, true, true, true, true, true, true};
	quickSegment.length_of_a_bit.length = 1;
	quickSegment.length_of_a_bit.clock_rate = 100;

	slowSegment.data = {true, true, true, true, true, true, true, true};
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
	sync_segment.data.resize(10*8);
	std::fill(sync_segment.data.begin(), sync_segment.data.end(), true);

	Storage::Disk::PCMSegment header_segment;
	header_segment.data.resize(14*8);
	std::fill(header_segment.data.begin(), header_segment.data.end(), true);

	Storage::Disk::PCMSegment data_segment;
	data_segment.data.resize(349*8);
	std::fill(data_segment.data.begin(), data_segment.data.end(), true);

	for(std::size_t c = 0; c < 16; ++c) {
		segments.push_back(sync_segment);
		segments.push_back(header_segment);
		segments.push_back(sync_segment);
		segments.push_back(data_segment);
		segments.push_back(sync_segment);
	}

	Storage::Disk::PCMTrack track(segments);
	const float late_time = 967445.0f / 2045454.0f;
	const auto offset = track.seek_to(late_time);
	XCTAssert(offset <= late_time, "Found location should be at or before sought time");

	const auto difference = late_time - offset;
	XCTAssert(difference >= 0.0 && difference < 0.005, "Next event should occur soon");

	XCTAssert(offset >= 0.0 && offset < 0.5, "Next event should occur soon");

	auto next_event = track.get_next_event();
	double next_event_duration = next_event.length.get<double>();
	XCTAssert(next_event_duration >= 0.0 && next_event_duration < 0.005, "Next event should occur soon");
}

@end
