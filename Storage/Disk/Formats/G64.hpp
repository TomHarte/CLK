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

namespace Storage {

class PCMTrack: public Track {
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
