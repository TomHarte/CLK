//
//  SingleTrackDisk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef SingleTrackDisk_hpp
#define SingleTrackDisk_hpp

#include "Disk.hpp"

namespace Storage {
namespace Disk {

/*!
	Provides a disk that has houses a single track.
*/
class SingleTrackDisk: public Disk {
	public:
		/// Constructs a single-track disk with the track @c track.
		SingleTrackDisk(const std::shared_ptr<Track> &track);

	private:
		std::shared_ptr<Track> track_;

		unsigned int get_head_position_count();
		std::shared_ptr<Track> get_uncached_track_at_position(unsigned int head, unsigned int position);
};

}
}

#endif /* SingleTrackDisk_hpp */
