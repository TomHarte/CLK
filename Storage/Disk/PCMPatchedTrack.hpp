//
//  PCMPatchedTrack.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/12/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef PCMPatchedTrack_hpp
#define PCMPatchedTrack_hpp

#include "PCMTrack.hpp"

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
			Replaces whatever is currently on the track from @c start_position to @c start_position + segment length
			with the contents of @c segment.
		*/
		void add_segment(const Time &start_position, const PCMSegment &segment);

		// To satisfy Storage::Disk::Track
		Event get_next_event();
		Time seek_to(const Time &time_since_index_hole);

	private:
		std::shared_ptr<Track> underlying_track_;
		struct Patch {
			Time start_position;
			PCMSegment segment;
			Patch(const Time &start_position, const PCMSegment &segment) : start_position(start_position), segment(segment) {}
		};
		std::vector<Patch> patches_;
		size_t active_patch_;
};

}
}

#endif /* PCMPatchedTrack_hpp */
