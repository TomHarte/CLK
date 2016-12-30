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
	segment_(new PCMSegment(segment))
{
	// add an extra bit of storage at the bottom if one is going to be needed;
	// events returned are going to be in integral multiples of the length of a bit
	// other than the very first and very last which will include a half bit length
	if(segment_->length_of_a_bit.length&1)
	{
		segment_->length_of_a_bit.length <<= 1;
		segment_->length_of_a_bit.clock_rate <<= 1;
	}

	// load up the clock rate once only
	next_event_.length.clock_rate = segment_->length_of_a_bit.clock_rate;

	// set initial conditions
	reset();
}

PCMSegmentEventSource::PCMSegmentEventSource(const PCMSegmentEventSource &original)
{
	// share underlying data with the original
	segment_ = original.segment_;

	// load up the clock rate and set initial conditions
	next_event_.length.clock_rate = segment_->length_of_a_bit.clock_rate;
	reset();
}

void PCMSegmentEventSource::reset()
{
	// start with the first bit to be considered the zeroth, and assume that it'll be
	// flux transitions for the foreseeable
	bit_pointer_ = 0;
	next_event_.type = Track::Event::FluxTransition;
}

Storage::Disk::Track::Event PCMSegmentEventSource::get_next_event()
{
	// track the initial bit pointer for potentially considering whether this was an
	// initial index hole or a subsequent one later on
	size_t initial_bit_pointer = bit_pointer_;

	// if starting from the beginning, pull half a bit backward, as if the initial bit
	// is set, it should be in the centre of its window
	next_event_.length.length = bit_pointer_ ? 0 : -(segment_->length_of_a_bit.length >> 1);

	// search for the next bit that is set, if any
	const uint8_t *segment_data = segment_->data.data();
	while(bit_pointer_ < segment_->number_of_bits)
	{
		int bit = segment_data[bit_pointer_ >> 3] & (0x80 >> (bit_pointer_&7));
		bit_pointer_++;	// so this always points one beyond the most recent bit returned
		next_event_.length.length += segment_->length_of_a_bit.length;

		// if this bit is set, return the event
		if(bit) return next_event_;
	}

	// if the end is reached without a bit being set, it'll be index holes from now on
	next_event_.type = Track::Event::IndexHole;

	// test whether this is the very first time that bits have been exhausted. If so then
	// allow an extra half bit's length to run from the position of the potential final transition
	// event to the end of the segment. Otherwise don't allow any extra time, as it's already
	// been consumed
	if(initial_bit_pointer <= segment_->number_of_bits)
	{
		next_event_.length.length += (segment_->length_of_a_bit.length >> 1);
		bit_pointer_++;
	}
	return next_event_;
}

Storage::Time PCMSegmentEventSource::get_length()
{
	return segment_->length_of_a_bit * segment_->number_of_bits;
}

Storage::Time PCMSegmentEventSource::seek_to(const Time &time_from_start)
{
	// test for requested time being beyond the end
	Time length = get_length();
	if(time_from_start >= length)
	{
		next_event_.type = Track::Event::IndexHole;
		bit_pointer_ = segment_->number_of_bits+1;
		return length;
	}

	// if not beyond the end then make an initial assumption that the next thing encountered will be a flux transition
	next_event_.type = Track::Event::FluxTransition;

	// test for requested time being before the first bit
	Time half_bit_length = segment_->length_of_a_bit;
	half_bit_length.length >>= 1;
	if(time_from_start < half_bit_length)
	{
		bit_pointer_ = 0;
		Storage::Time zero;
		return zero;
	}

	// adjust for time to get to bit zero and determine number of bits in;
	// bit_pointer_ always records _the next bit_ that might trigger an event,
	// so should be one beyond the one reached by a seek.
	Time relative_time = time_from_start - half_bit_length;
	bit_pointer_ = 1 + (relative_time / segment_->length_of_a_bit).get_unsigned_int();

	// map up to the correct amount of time
	return half_bit_length + segment_->length_of_a_bit * (unsigned int)(bit_pointer_ - 1);
}
