//
//  TrackSerialiser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "TrackSerialiser.hpp"

#include <memory>

// TODO: if this is a PCMTrack with only one segment and that segment's bit rate is within tolerance,
// just return a copy of that segment.
Storage::Disk::PCMSegment Storage::Disk::track_serialisation(const Track &track, Time length_of_a_bit) {
	unsigned int history_size = 16;
	std::unique_ptr<Track> track_copy(track.clone());

	// ResultAccumulator exists to append whatever comes out of the PLL to
	// its PCMSegment.
	struct ResultAccumulator {
		PCMSegment result;
		void digital_phase_locked_loop_output_bit(int value) {
			result.data.push_back(!!value);
		}
	} result_accumulator;
	result_accumulator.result.length_of_a_bit = length_of_a_bit;
	DigitalPhaseLockedLoop<ResultAccumulator> pll(100);

	// Obtain a length multiplier which is 100 times the reciprocal
	// of the expected bit length. So a perfect bit length from
	// the source data will come out as 100 ticks.
	Time length_multiplier = Time(100*length_of_a_bit.clock_rate, length_of_a_bit.length);
	length_multiplier.simplify();

	// start at the index hole
	track_copy->seek_to(Time(0));

	// grab events until the next index hole
	Time time_error = Time(0);
	while(true) {
		Track::Event next_event = track_copy->get_next_event();

		Time extended_length = next_event.length * length_multiplier + time_error;
		time_error.clock_rate = extended_length.clock_rate;
		time_error.length = extended_length.length % extended_length.clock_rate;
		pll.run_for(Cycles(static_cast<int>(extended_length.get<int64_t>())));

		if(next_event.type == Track::Event::IndexHole) break;
		pll.add_pulse();

		// If the PLL is now sufficiently primed, restart, and start recording bits this time.
		if(history_size) {
			history_size--;
			if(!history_size) {
				track_copy->seek_to(Time(0));
				time_error.set_zero();
				pll.set_delegate(&result_accumulator);
			}
		}
	}

	return result_accumulator.result;
}
