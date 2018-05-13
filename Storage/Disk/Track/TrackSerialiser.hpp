//
//  TrackSerialiser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef TrackSerialiser_h
#define TrackSerialiser_h

#include "../DPLL/DigitalPhaseLockedLoop.hpp"
#include "PCMSegment.hpp"
#include "Track.hpp"

namespace Storage {
namespace Disk {

/*!
	Instantiates a PLL with a target bit length of @c length_of_a_bit and provides a complete
	serialisation of @c track, starting from the index hole.

	This feature is offered for the benefit of various parts of the code that need to make
	sense of a track **other than emulation**, as it renders a one-off image of the track,
	which can be inaccurate. However there are many occasions where a single rendering is
	desireable, e.g. file formats that apply that constraint, or static analysis prior to
	emulation launch, which works with broad strokes.

	@param track The track to serialise.
	@param length_of_a_bit The expected length of a single bit, as a proportion of the
	track length.
*/
PCMSegment track_serialisation(Track &track, Time length_of_a_bit);

}
}

#endif /* TrackSerialiser_h */
