//
//  PCMSegment.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/12/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "PCMSegment.hpp"

#include <cassert>
#include <cstdlib>

using namespace Storage::Disk;

PCMSegmentEventSource::PCMSegmentEventSource(const PCMSegment &segment) :
		segment_(new PCMSegment(segment)) {
	// add an extra bit of storage at the bottom if one is going to be needed;
	// events returned are going to be in integral multiples of the length of a bit
	// other than the very first and very last which will include a half bit length
	if(segment_->length_of_a_bit.length&1) {
		segment_->length_of_a_bit.length <<= 1;
		segment_->length_of_a_bit.clock_rate <<= 1;
	}

	// load up the clock rate once only
	next_event_.length.clock_rate = segment_->length_of_a_bit.clock_rate;

	// set initial conditions
	reset();
}

PCMSegmentEventSource::PCMSegmentEventSource(const PCMSegmentEventSource &original) {
	// share underlying data with the original
	segment_ = original.segment_;

	// load up the clock rate and set initial conditions
	next_event_.length.clock_rate = segment_->length_of_a_bit.clock_rate;
	reset();
}

void PCMSegmentEventSource::reset() {
	// start with the first bit to be considered the zeroth, and assume that it'll be
	// flux transitions for the foreseeable
	bit_pointer_ = 0;
	next_event_.type = Track::Event::FluxTransition;
}

PCMSegment &PCMSegment::operator +=(const PCMSegment &rhs) {
	data.insert(data.end(), rhs.data.begin(), rhs.data.end());
	return *this;
}

void PCMSegment::rotate_right(size_t length) {
	length %= data.size();
	if(!length) return;

	// To rotate to the right, front-insert the proper number
	// of bits from the end and then resize. To rotate to
	// the left, do the opposite.
	std::vector<uint8_t> data_copy;
	if(length > 0) {
		data_copy.insert(data_copy.end(), data.end() - off_t(length), data.end());
		data.erase(data.end() - off_t(length), data.end());
		data.insert(data.begin(), data_copy.begin(), data_copy.end());
	} else {
		data_copy.insert(data_copy.end(), data.begin(), data.begin() - off_t(length));
		data.erase(data.begin(), data.begin() - off_t(length));
		data.insert(data.end(), data_copy.begin(), data_copy.end());
	}
}

Storage::Disk::Track::Event PCMSegmentEventSource::get_next_event() {
	// Track the initial bit pointer for potentially considering whether this was an
	// initial index hole or a subsequent one later on.
	const std::size_t initial_bit_pointer = bit_pointer_;

	// If starting from the beginning, pull half a bit backward, as if the initial bit
	// is set, it should be in the centre of its window.
	next_event_.length.length = bit_pointer_ ? 0 : -(segment_->length_of_a_bit.length >> 1);

	// search for the next bit that is set, if any
	while(bit_pointer_ < segment_->data.size()) {
		bool bit = segment_->data[bit_pointer_];
		++bit_pointer_;	// so this always points one beyond the most recent bit returned
		next_event_.length.length += segment_->length_of_a_bit.length;

		// if this bit is set, or is fuzzy and a random bit of 1 is selected, return the event.
		if(bit ||
			(!segment_->fuzzy_mask.empty() && segment_->fuzzy_mask[bit_pointer_] && lfsr_.next())
		)	return next_event_;
	}

	// If the end is reached without a bit being set, it'll be index holes from now on.
	next_event_.type = Track::Event::IndexHole;

	// Test whether this is the very first time that bits have been exhausted. If so then
	// allow an extra half bit's length to run from the position of the potential final transition
	// event to the end of the segment. Otherwise don't allow any extra time, as it's already
	// been consumed.
	if(initial_bit_pointer <= segment_->data.size()) {
		next_event_.length.length += (segment_->length_of_a_bit.length >> 1);
		bit_pointer_++;
	}
	return next_event_;
}

Storage::Time PCMSegmentEventSource::get_length() {
	return segment_->length_of_a_bit * unsigned(segment_->data.size());
}

float PCMSegmentEventSource::seek_to(float time_from_start) {
	// test for requested time being beyond the end
	const float length = get_length().get<float>();
	if(time_from_start >= length) {
		next_event_.type = Track::Event::IndexHole;
		bit_pointer_ = segment_->data.size()+1;
		return length;
	}

	// if not beyond the end then make an initial assumption that the next thing encountered will be a flux transition
	next_event_.type = Track::Event::FluxTransition;

	// test for requested time being before the first bit
	const float bit_length = segment_->length_of_a_bit.get<float>();
	const float half_bit_length = bit_length / 2.0f;
	if(time_from_start < half_bit_length) {
		bit_pointer_ = 0;
		return 0.0f;
	}

	// adjust for time to get to bit zero and determine number of bits in;
	// bit_pointer_ always records _the next bit_ that might trigger an event,
	// so should be one beyond the one reached by a seek.
	const float relative_time = time_from_start + half_bit_length;	// the period [0, 0.5) should map to window 0; [0.5, 1.5) should map to window 1; etc.
	bit_pointer_ = 1 + size_t(relative_time / bit_length);

	// map up to the correct amount of time
	return half_bit_length + segment_->length_of_a_bit.get<float>() * float(bit_pointer_ - 1);
}

const PCMSegment &PCMSegmentEventSource::segment() const {
	return *segment_;
}

PCMSegment &PCMSegmentEventSource::segment() {
	return *segment_;
}
