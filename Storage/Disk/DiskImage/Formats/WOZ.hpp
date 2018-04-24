//
//  WOZ.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef WOZ_hpp
#define WOZ_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides a @c DiskImage containing a WOZ — a bit stream representation of a floppy.
*/
class WOZ: public DiskImage {
	public:
		WOZ(const std::string &file_name);

		enum {
			ErrorNotWOZ
		};

		int get_head_position_count() override;
		int get_head_count() override;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) override;

	private:
		Storage::FileHolder file_;
		bool is_read_only_ = false;
		bool is_3_5_disk_ = false;
		uint8_t track_map_[160];
		long tracks_offset_ = 0;
};

}
}

#endif /* WOZ_hpp */
