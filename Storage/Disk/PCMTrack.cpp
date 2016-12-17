//
//  PCMTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "PCMTrack.hpp"
#include "../../NumberTheory/Factors.hpp"

using namespace Storage::Disk;

PCMTrack::PCMTrack(std::vector<PCMSegment> segments)
{
	segments_ = std::move(segments);
	fix_length();
}

PCMTrack::PCMTrack(PCMSegment segment)
{
	segment.length_of_a_bit.length = 1;
	segment.length_of_a_bit.clock_rate = 1;
	segments_.push_back(std::move(segment));
	fix_length();
}

Track::Event PCMTrack::get_next_event()
{
	// find the next 1 in the input stream, keeping count of length as we go, and assuming it's going
	// to be a flux transition
	next_event_.type = Track::Event::FluxTransition;
	next_event_.length.length = 0;
	while(segment_pointer_ < segments_.size())
	{
		unsigned int clock_multiplier = track_clock_rate_ / segments_[segment_pointer_].length_of_a_bit.clock_rate;
		unsigned int bit_length = clock_multiplier * segments_[segment_pointer_].length_of_a_bit.length;

		const uint8_t *segment_data = &segments_[segment_pointer_].data[0];
		while(bit_pointer_ < segments_[segment_pointer_].number_of_bits)
		{
			// for timing simplicity, bits are modelled as happening at the end of their window
			// TODO: should I account for the converse bit ordering? Or can I assume MSB first?
			int bit = segment_data[bit_pointer_ >> 3] & (0x80 >> (bit_pointer_&7));
			bit_pointer_++;
			next_event_.length.length += bit_length;

			if(bit) return next_event_;
		}
		bit_pointer_ = 0;
		segment_pointer_++;
	}

	// check whether we actually reached the index hole
	if(segment_pointer_ == segments_.size())
	{
		segment_pointer_ = 0;
		next_event_.type = Track::Event::IndexHole;
	}

	return next_event_;
}

Storage::Time PCMTrack::seek_to(const Time &time_since_index_hole)
{
	segment_pointer_ = 0;

	// pick a common clock rate for counting time on this track and multiply up the time being sought appropriately
	Time time_so_far;
	Time target_time = time_since_index_hole;
	time_so_far.clock_rate = NumberTheory::least_common_multiple(next_event_.length.clock_rate, target_time.clock_rate);
	target_time.length *= time_so_far.clock_rate / target_time.clock_rate;
	target_time.clock_rate = time_so_far.clock_rate;

	while(segment_pointer_ < segments_.size())
	{
		// determine how long this segment is in terms of the master clock
		unsigned int clock_multiplier = time_so_far.clock_rate / next_event_.length.clock_rate;
		unsigned int bit_length = ((clock_multiplier / track_clock_rate_) / segments_[segment_pointer_].length_of_a_bit.clock_rate) * segments_[segment_pointer_].length_of_a_bit.length;
		unsigned int time_in_this_segment = bit_length * segments_[segment_pointer_].number_of_bits;

		// if this segment goes on longer than the time being sought, end here
		unsigned int time_remaining = target_time.length - time_so_far.length;
		if(time_in_this_segment >= time_remaining)
		{
			// get the amount of time actually to move into this segment
			unsigned int time_found = time_remaining - (time_remaining % bit_length);

			// resolve that into the stateful bit count
			bit_pointer_ = 1 + (time_remaining / bit_length);

			// update and return the time sought to
			time_so_far.length += time_found;
			return time_so_far;
		}

		// otherwise, accumulate time and keep moving
		time_so_far.length += time_in_this_segment;
		segment_pointer_++;
	}
	return target_time;
}

void PCMTrack::fix_length()
{
	// find the least common multiple of all segment clock rates
	track_clock_rate_ = segments_[0].length_of_a_bit.clock_rate;
	for(size_t c = 1; c < segments_.size(); c++)
	{
		track_clock_rate_ = NumberTheory::least_common_multiple(track_clock_rate_, segments_[c].length_of_a_bit.clock_rate);
	}

	// thereby determine the total length, storing it to next_event as the track-total divisor
	next_event_.length.clock_rate = 0;
	for(size_t c = 0; c < segments_.size(); c++)
	{
		unsigned int multiplier = track_clock_rate_ / segments_[c].length_of_a_bit.clock_rate;
		next_event_.length.clock_rate += segments_[c].length_of_a_bit.length * segments_[c].number_of_bits * multiplier;
	}

	segment_pointer_ = bit_pointer_ = 0;
}
