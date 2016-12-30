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
#include "PCMSegment.hpp"
#include <vector>

namespace Storage {
namespace Disk {

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
		PCMTrack(const std::vector<PCMSegment> &);

		/*!
			Creates a @c PCMTrack consisting of a single continuous run of data, implying a constant clock rate.
			The segment's @c length_of_a_bit will be ignored and therefore need not be filled in.
		*/
		PCMTrack(const PCMSegment &);

		/*!
			Copy constructor; required for Tracks in order to support modifiable disks.
		*/
		PCMTrack(const PCMTrack &);

		// as per @c Track
		Event get_next_event();
		Time seek_to(const Time &time_since_index_hole);
		Track *clone();

	private:
		// storage for the segments that describe this track
		std::vector<PCMSegmentEventSource> segment_event_sources_;

		// a pointer to the first bit to consider as the next event
		size_t segment_pointer_;

		PCMTrack();
};

}
}

#endif /* PCMTrack_hpp */
