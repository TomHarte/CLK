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

Storage::Time PCMSegmentEventSource::get_length()
{
	return segment_.length_of_a_bit * segment_.number_of_bits;
}

Storage::Time PCMSegmentEventSource::seek_to(const Time &time_from_start)
{
	// test for requested time being beyond the end
	Time length = get_length();
	if(time_from_start >= length)
	{
		next_event_.type = Track::Event::IndexHole;
		bit_pointer_ = segment_.number_of_bits;
		return length;
	}

	// if not beyond the end then make an initial assumption that the next thing encountered will be a flux transition
	next_event_.type = Track::Event::FluxTransition;

	// test for requested time being before the first bit
	Time half_bit_length = segment_.length_of_a_bit;
	half_bit_length.length >>= 1;
	if(time_from_start < half_bit_length)
	{
		bit_pointer_ = 0;
		Storage::Time zero;
		return zero;
	}

	// adjust for time to get to bit zero and determine number of bits in
	Time relative_time = time_from_start - half_bit_length;
	bit_pointer_ = (relative_time / segment_.length_of_a_bit).get_unsigned_int();

	// map up to the correct amount of time
	return half_bit_length + segment_.length_of_a_bit * (unsigned int)bit_pointer_;
}
