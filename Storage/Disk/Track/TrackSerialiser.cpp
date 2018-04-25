//
//  TrackSerialiser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "TrackSerialiser.hpp"

// TODO: if this is a PCMTrack with only one segment and that segment's bit rate is within tolerance,
// just return a copy of that segment.
Storage::Disk::PCMSegment Storage::Disk::track_serialisation(Track &track, Time length_of_a_bit) {
	unsigned int history_size = 16;
	DigitalPhaseLockedLoop pll(100, history_size);

	struct ResultAccumulator: public DigitalPhaseLockedLoop::Delegate {
		PCMSegment result;
		void digital_phase_locked_loop_output_bit(int value) {
			result.data.resize(1 + (result.number_of_bits >> 3));
			if(value) result.data[result.number_of_bits >> 3] |= 0x80 >> (result.number_of_bits & 7);
			result.number_of_bits++;
		}
	} result_accumulator;
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
		pll.run_for(Cycles(extended_length.get<int>()));
		pll.add_pulse();

		// If the PLL is now sufficiently primed, restart, and start recording bits this time.
		if(history_size) {
			history_size--;
			if(!history_size) {
				track.seek_to(Time(0));
				time_error.set_zero();
				pll.set_delegate(&result_accumulator);
			}
		}
	}

	return result_accumulator.result;
}
