//
//  PCMSegment.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "PCMSegment.hpp"

using namespace Storage::Disk;

PCMSegmentEventSource::PCMSegmentEventSource(const PCMSegment &segment) :
	segment_(segment)
{
	if(segment_.length_of_a_bit.length&1)
	{
		segment_.length_of_a_bit.length <<= 1;
		segment_.length_of_a_bit.clock_rate <<= 1;
	}
	next_event_.length.clock_rate = segment_.length_of_a_bit.clock_rate;
	reset();
}

void PCMSegmentEventSource::reset()
{
	bit_pointer_ = 0;
	next_event_.type = Track::Event::FluxTransition;
}

Storage::Disk::Track::Event PCMSegmentEventSource::get_next_event()
{
	size_t initial_bit_pointer = bit_pointer_;
	next_event_.length.length = bit_pointer_ ? 0 : -(segment_.length_of_a_bit.length >> 1);

	const uint8_t *segment_data = segment_.data.data();
	while(bit_pointer_ < segment_.number_of_bits)
	{
		int bit = segment_data[bit_pointer_ >> 3] & (0x80 >> (bit_pointer_&7));
		bit_pointer_++;
		next_event_.length.length += segment_.length_of_a_bit.length;

		if(bit) return next_event_;
	}

	if(initial_bit_pointer < segment_.number_of_bits) next_event_.length.length += (segment_.length_of_a_bit.length >> 1);
	next_event_.type = Track::Event::IndexHole;
	return next_event_;
}
