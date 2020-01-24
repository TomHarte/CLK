//
//  PCMTrack.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef PCMTrack_hpp
#define PCMTrack_hpp

#include "Track.hpp"
#include "PCMSegment.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"

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

		/*!
			Creates a PCMTrack by sampling the original at a rate of @c bits_per_track.
		*/
		static PCMTrack *resampled_clone(Track *original, size_t bits_per_track);

		// as per @c Track
		Event get_next_event() final;
		Time seek_to(const Time &time_since_index_hole) final;
		Track *clone() const final;

		// Obtains a copy of this track, flattened to a single PCMSegment, which
		// consists of @c bits_per_track potential flux transition points.
		PCMTrack *resampled_clone(size_t bits_per_track);
		bool is_resampled_clone();

		/*!
			Replaces whatever is currently on the track from @c start_position to @c start_position + segment length
			with the contents of @c segment.

			This is a well-defined operation only for tracks with a single segment. The new segment will be resampled
			to the track's underlying segment, which will be mutated.

			@param start_time The time at which this segment begins. Must be in the range [0, 1).
			@param segment The PCM segment to add.
			@param clamp_to_index_hole If @c true then the new segment will be truncated if it overruns the index hole;
				it will otherwise write over the index hole and continue.
		*/
		void add_segment(const Time &start_time, const PCMSegment &segment, bool clamp_to_index_hole);

	private:
		/*!
			Creates a PCMTrack with a single segment, consisting of @c bits_per_track flux windows,
			initialised with no flux events.
		*/
		PCMTrack(unsigned int bits_per_track);

		// storage for the segments that describe this track
		std::vector<PCMSegmentEventSource> segment_event_sources_;

		// a pointer to the first bit to consider as the next event
		std::size_t segment_pointer_;

		PCMTrack();
		bool is_resampled_clone_ = false;
};

}
}

#endif /* PCMTrack_hpp */
