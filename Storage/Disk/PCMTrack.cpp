//
//  PCMTrack.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "PCMTrack.hpp"
#include "../../NumberTheory/Factors.hpp"

using namespace Storage;

PCMTrack::PCMTrack(std::vector<PCMSegment> segments)
{
	_segments = std::move(segments);
	fix_length();
}

PCMTrack::PCMTrack(PCMSegment segment)
{
	_segments.push_back(std::move(segment));
	fix_length();
}

PCMTrack::Event PCMTrack::get_next_event()
{
	// find the next 1 in the input stream, keeping count of length as we go, and assuming it's going
	// to be a flux transition
	_next_event.type = Track::Event::FluxTransition;
	_next_event.length.length = 0;
	while(_segment_pointer < _segments.size())
	{
		unsigned int clock_multiplier = _track_clock_rate / _segments[_segment_pointer].length_of_a_bit.clock_rate;
		const uint8_t *segment_data = _segments[_segment_pointer].data.get();
		while(_bit_pointer < _segments[_segment_pointer].number_of_bits)
		{
			// for timing simplicity, bits are modelled as happening at the end of their window
			// TODO: should I account for the converse bit ordering? Or can I assume MSB first?
			int bit = segment_data[_bit_pointer >> 3] & (0x80 >> (_bit_pointer&7));
			_bit_pointer++;
			_next_event.length.length += clock_multiplier;

			if(bit) return _next_event;
		}
		_bit_pointer = 0;
		_segment_pointer++;
	}

	// check whether we actually reached the index hole
	if(_segment_pointer == _segments.size())
	{
		_segment_pointer = 0;
		_next_event.type = Track::Event::IndexHole;
	}

	return _next_event;
}

void PCMTrack::fix_length()
{
	// find the least common multiple of all segment clock rates
	_track_clock_rate = _segments[0].length_of_a_bit.clock_rate;
	for(size_t c = 1; c < _segments.size(); c++)
	{
		_track_clock_rate = NumberTheory::least_common_multiple(_track_clock_rate, _segments[c].length_of_a_bit.clock_rate);
	}

	// thereby determine the total length, storing it to next_event as the track-total divisor
	_next_event.length.clock_rate = 0;
	for(size_t c = 0; c < _segments.size(); c++)
	{
		unsigned int multiplier = _track_clock_rate / _segments[c].length_of_a_bit.clock_rate;
		_next_event.length.clock_rate += _segments[c].length_of_a_bit.length * _segments[c].number_of_bits * multiplier;
	}

	_segment_pointer = _bit_pointer = 0;
}
