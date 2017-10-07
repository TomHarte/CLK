//
//  MFMSectorDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef SectorDump_hpp
#define SectorDump_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage {
namespace Disk {

/*!
	Provies the base for writeable [M]FM disk images that just contain contiguous sector content dumps.
*/
class MFMSectorDump: public DiskImage, public Storage::FileHolder {
	public:
		MFMSectorDump(const char *file_name);
		void set_geometry(int sectors_per_track, uint8_t sector_size, bool is_double_density);

		using Storage::FileHolder::get_is_read_only;
		void set_track_at_position(Track::Address address, const std::shared_ptr<Track> &track);
		std::shared_ptr<Track> get_track_at_position(Track::Address address);

	private:
		std::mutex file_access_mutex_;
		virtual long get_file_offset_for_position(Track::Address address) = 0;

		int sectors_per_track_ = 0;
		uint8_t sector_size_ = 0;
		bool is_double_density_ = true;
};

}
}

#endif /* SectorDump_hpp */
