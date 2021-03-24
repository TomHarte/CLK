//
//  MFMSectorDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef SectorDump_hpp
#define SectorDump_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage {
namespace Disk {

/*!
	Provides the base for writeable [M]FM disk images that just contain contiguous sector content dumps.
*/
class MFMSectorDump: public DiskImage {
	public:
		MFMSectorDump(const std::string &file_name);
		void set_geometry(int sectors_per_track, uint8_t sector_size, uint8_t first_sector, bool is_double_density);

		bool get_is_read_only() final;
		void set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) final;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) final;

	protected:
		Storage::FileHolder file_;

	private:
		virtual long get_file_offset_for_position(Track::Address address) = 0;

		int sectors_per_track_ = 0;
		uint8_t sector_size_ = 0;
		bool is_double_density_ = true;
		uint8_t first_sector_ = 0;
};

}
}

#endif /* SectorDump_hpp */
