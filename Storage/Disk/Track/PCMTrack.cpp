//
//  PCMTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "PCMTrack.hpp"
#include "../../../NumberTheory/Factors.hpp"

using namespace Storage::Disk;

PCMTrack::PCMTrack() : segment_pointer_(0) {}

PCMTrack::PCMTrack(const std::vector<PCMSegment> &segments) : PCMTrack() {
	// sum total length of all segments
	Time total_length;
	for(const auto &segment : segments) {
		total_length += segment.length_of_a_bit * segment.number_of_bits;
	}
	total_length.simplify();

	// each segment is then some proportion of the total; for them all to sum to 1 they'll
	// need to be adjusted to be
	for(const auto &segment : segments) {
		Time original_length_of_segment = segment.length_of_a_bit * segment.number_of_bits;
		Time proportion_of_whole = original_length_of_segment / total_length;
		proportion_of_whole.simplify();
		PCMSegment length_adjusted_segment = segment;
		length_adjusted_segment.length_of_a_bit = proportion_of_whole / segment.number_of_bits;
		length_adjusted_segment.length_of_a_bit.simplify();
		segment_event_sources_.emplace_back(length_adjusted_segment);
	}
}

PCMTrack::PCMTrack(const PCMSegment &segment) : PCMTrack() {
	// a single segment necessarily fills the track
	PCMSegment length_adjusted_segment = segment;
	length_adjusted_segment.length_of_a_bit.length = 1;
	length_adjusted_segment.length_of_a_bit.clock_rate = segment.number_of_bits;
	segment_event_sources_.emplace_back(length_adjusted_segment);
}

PCMTrack::PCMTrack(const PCMTrack &original) : PCMTrack() {
	segment_event_sources_ = original.segment_event_sources_;
}

Track *PCMTrack::clone() {
	return new PCMTrack(*this);
}

Track::Event PCMTrack::get_next_event() {
	// ask the current segment for a new event
	Track::Event event = segment_event_sources_[segment_pointer_].get_next_event();

	// if it was a flux transition, that's code for end-of-segment, so dig deeper
	if(event.type == Track::Event::IndexHole) {
		// multiple segments may be crossed, so start summing lengths in case the net
		// effect is an index hole
		Time total_length = event.length;

		// continue until somewhere no returning an index hole
		while(event.type == Track::Event::IndexHole) {
			// advance to the [start of] the next segment
			segment_pointer_ = (segment_pointer_ + 1) % segment_event_sources_.size();
			segment_event_sources_[segment_pointer_].reset();

			// if this is all the way back to the start, that's a genuine index hole,
			// so set the summed length and return
			if(!segment_pointer_) {
				return event;
			}

			// otherwise get the next event (if it's not another index hole, the loop will end momentarily),
			// summing in any prior accumulated time
			event = segment_event_sources_[segment_pointer_].get_next_event();
			total_length += event.length;
			event.length = total_length;
		}
	}

	return event;
}

Storage::Time PCMTrack::seek_to(const Time &time_since_index_hole) {
	// initial condition: no time yet accumulated, the whole thing requested yet to navigate
	Storage::Time accumulated_time;
	Storage::Time time_left_to_seek = time_since_index_hole;

	// search from the first segment
	segment_pointer_ = 0;
	do {
		// if this segment extends beyond the amount of time left to seek, trust it to complete
		// the seek
		Storage::Time segment_time = segment_event_sources_[segment_pointer_].get_length();
		if(segment_time > time_left_to_seek) {
			return accumulated_time + segment_event_sources_[segment_pointer_].seek_to(time_left_to_seek);
		}

		// otherwise swallow this segment, updating the time left to seek and time so far accumulated
		time_left_to_seek -= segment_time;
		accumulated_time += segment_time;
		segment_pointer_ = (segment_pointer_ + 1) % segment_event_sources_.size();
	} while(segment_pointer_);

	// if all segments have now been swallowed, the closest we can get is the very end of
	// the list of segments
	return accumulated_time;
}
