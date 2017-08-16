//
//  PCMPatchedTrack.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef PCMPatchedTrack_hpp
#define PCMPatchedTrack_hpp

#include "Disk.hpp"
#include "PCMSegment.hpp"

namespace Storage {
namespace Disk {

/*!
	A subclass of @c Track that patches an existing track with PCM segments.
*/
class PCMPatchedTrack: public Track {
	public:
		/*!
			Constructs a @c PCMPatchedTrack that will return events from @c underlying_track in
			regions where it has not had alternative PCM data installed.
		*/
		PCMPatchedTrack(std::shared_ptr<Track> underlying_track);

		/*!
			Copy constructor, for Track.
		*/
		PCMPatchedTrack(const PCMPatchedTrack &);

		/*!
			Replaces whatever is currently on the track from @c start_position to @c start_position + segment length
			with the contents of @c segment.
			
			@param start_time The time at which this segment begins. Must be in the range [0, 1).
			@param segment The PCM segment to add.
			@param clamp_to_index_hole If @c true then the new segment will be truncated if it overruns the index hole;
			it will otherwise write over the index hole and continue.
		*/
		void add_segment(const Time &start_time, const PCMSegment &segment, bool clamp_to_index_hole);

		// To satisfy Storage::Disk::Track
		Event get_next_event();
		Time seek_to(const Time &time_since_index_hole);
		Track *clone();

	private:
		std::shared_ptr<Track> underlying_track_;

		struct Period {
			Time start_time, end_time;
			Time segment_start_time;
			std::shared_ptr<PCMSegmentEventSource> event_source; // nullptr => use the underlying track

			void push_start_to_time(const Storage::Time &new_start_time);
			void trim_end_to_time(const Storage::Time &new_end_time);

			Period(const Time &start_time, const Time &end_time, const Time &segment_start_time, std::shared_ptr<PCMSegmentEventSource> event_source) :
				start_time(start_time), end_time(end_time), segment_start_time(segment_start_time), event_source(event_source) {}
			Period(const Period &);
		};
		std::vector<Period> periods_;
		std::vector<Period>::iterator active_period_;
		Time current_time_, insertion_error_;

		void insert_period(const Period &period);
};

}
}

#endif /* PCMPatchedTrack_hpp */
