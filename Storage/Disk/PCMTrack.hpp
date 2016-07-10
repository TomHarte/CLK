//
//  PCMTrack.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef PCMTrack_hpp
#define PCMTrack_hpp

#include "Disk.hpp"
#include <vector>

namespace Storage {

/*!
	A segment of PCM-sampled data. The clock rate in the duration is taken to be relative to all other
	segments that comprise a track rather than absolute, and the length is taken to be the number of
	bits from @c data that are actually present.

	Bits from each byte are taken MSB to LSB.

	Actual segment lengths will be calculated such that all segments that comprise a track exactly fill the track.

	So the segment for a track with only a single segment may supply any clock rate other than 0. It will exactly
	fill the track, so if it has 7 samples then there will be at most a flux transition every 1/7th of a rotation.

	If a track consists of two segments, one with clock rate 1 and one with clock rate 2, the second will be
	clocked twice as fast as the first.
*/
struct PCMSegment {
	Time duration;
	std::unique_ptr<uint8_t> data;
};

/*!
	A subclass of @c Track that provides its @c Events by querying a pulse-code modulated record of original
	flux detections, with an implied index hole at the very start of the data.

	The data may consist of a single @c PCMSegment or of multiple, allowing a PCM-format track to contain
	multiple distinct segments of data, each with a separate clock rate.
*/
class PCMTrack: public Track {
	public:
		/*!
			Creates a @c PCMTrack consisting of multiple segments of data, permitting multiple clock rates.
		*/
		PCMTrack(std::vector<PCMSegment> segments);

		/*!
			Creates a @c PCMTrack consisting of a single continuous run of data, implying a constant clock rate.
		*/
		PCMTrack(PCMSegment segment);

		// as per @c Track
		Event get_next_event();

	private:
		// storage for the segments that describe this track
		std::vector<PCMSegment> _segments;

		// a helper to determine the overall track clock rate and it's length
		void fix_length();

		// the event perpetually returned; impliedly contains the length of the entire track
		// as its clock rate, per the need for everything on a Track to sum to a length of 1
		PCMTrack::Event _next_event;

		// contains the master clock rate
		unsigned int _track_clock_rate;

		// a pointer to the first bit to consider as the next event
		size_t _segment_pointer;
		size_t _bit_pointer;
};

}

#endif /* PCMTrack_hpp */
