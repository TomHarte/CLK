//
//  TrackSerialiser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef TrackSerialiser_h
#define TrackSerialiser_h

#include "../DPLL/DigitalPhaseLockedLoop.hpp"

namespace Storage {
namespace Disk {

/*!
	Instantiates a PLL with a target bit length of @c length_of_a_bit and provides a complete
	serialisation of @c track, starting from the index hole.

	This feature is offered for the benefit of various parts of the code that need to make
	sense of a track **other than emulation**, as it renders a one-off image of the track,
	which can be inaccurate. However there are many occasions where a single rendering is
	desireable — e.g. file formats that apply that constraint, or static analysis prior to
	emulation launch, which works with broad strokes.

	@param track The track to serialise.
	@param length_of_a_bit The expected length of a single bit, as a proportion of the
	track length.
*/
template <typename T> PCMSegment track_serialisation(T &track, Time length_of_a_bit) {
	DigitalPhaseLockedLoop pll(100, 16);

	struct ResultAccumulator: public DigitalPhaseLockedLoop::Delegate {
		PCMSegment result;
		void digital_phase_locked_loop_output_bit(int value) {
			result.data.resize(1 + (result.number_of_bits >> 3));
			if(value) result.data[result.number_of_bits >> 3] |= 0x80 >> (result.number_of_bits & 7);
			result.number_of_bits++;
		}
	} result_accumulator;
	pll.set_delegate(&result_accumulator);
	result_accumulator.result.number_of_bits = 0;
	result_accumulator.result.length_of_a_bit = length_of_a_bit;

	Time length_multiplier = Time(100*length_of_a_bit.clock_rate, length_of_a_bit.length);
	length_multiplier.simplify();

	// start at the index hole
	track.seek_to(Time(0));

	// grab events until the next index hole
	Time time_error = Time(0);
	while(true) {
		Track::Event next_event = track.get_next_event();
		if(next_event.type == Track::Event::IndexHole) break;

		Time extended_length = next_event.length * length_multiplier + time_error;
		time_error.clock_rate = extended_length.clock_rate;
		time_error.length = extended_length.length % extended_length.clock_rate;
		pll.run_for(Cycles((int)extended_length.get_unsigned_int()));
		pll.add_pulse();
	}

	return result_accumulator.result;
}

}
}

#endif /* TrackSerialiser_h */
