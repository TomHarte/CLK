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
namespace Disk {

/*!
	A segment of PCM-sampled data.

	Bits from each byte are taken MSB to LSB.
*/
struct PCMSegment {
	Time length_of_a_bit;
	unsigned int number_of_bits;
	std::vector<uint8_t> data;
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
			The segment's @c length_of_a_bit will be ignored and therefore need not be filled in.
		*/
		PCMTrack(PCMSegment segment);

		// as per @c Track
		Event get_next_event();
		Time seek_to(Time time_since_index_hole);

	private:
		// storage for the segments that describe this track
		std::vector<PCMSegment> segments_;

		// a helper to determine the overall track clock rate and it's length
		void fix_length();

		// the event perpetually returned; impliedly contains the length of the entire track
		// as its clock rate, per the need for everything on a Track to sum to a length of 1
		PCMTrack::Event next_event_;

		// contains the master clock rate
		unsigned int track_clock_rate_;

		// a pointer to the first bit to consider as the next event
		size_t segment_pointer_;
		size_t bit_pointer_;
};

}
}

#endif /* PCMTrack_hpp */
