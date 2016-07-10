//
//  G64.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef G64_hpp
#define G64_hpp

#include "../Disk.hpp"
#include <vector>

namespace Storage {

struct PCMSegment {
	Time duration;
	std::unique_ptr<uint8_t> data;
};

class PCMTrack: public Track {
	public:
		PCMTrack(std::vector<PCMSegment> segments);
		PCMTrack(PCMSegment segment);

		virtual Event get_next_event();

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

class G64: public Disk {
	public:
		G64(const char *file_name);
		~G64();

		enum {
			ErrorNotGCR,
			ErrorUnknownVersion
		};

		unsigned int get_head_position_count();
		std::shared_ptr<Track> get_track_at_position(unsigned int position);

	private:
		FILE *_file;

		uint8_t _number_of_tracks;
		uint16_t _maximum_track_size;
};

};

#endif /* G64_hpp */
