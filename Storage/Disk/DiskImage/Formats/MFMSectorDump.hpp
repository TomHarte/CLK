//
//  MFMSectorDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"
#include "../../Encodings/MFM/Constants.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides the base for writeable [M]FM disk images that just contain contiguous sector content dumps.
*/
class MFMSectorDump: public DiskImage {
	public:
		MFMSectorDump(const std::string &file_name);

		bool get_is_read_only() final;
		void set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) final;
		std::shared_ptr<Track> get_track_at_position(Track::Address address) final;

	protected:
		Storage::FileHolder file_;
		void set_geometry(int sectors_per_track, uint8_t sector_size, uint8_t first_sector, Encodings::MFM::Density density);

	private:
		virtual long get_file_offset_for_position(Track::Address address) = 0;

		int sectors_per_track_ = 0;
		uint8_t sector_size_ = 0;
		Encodings::MFM::Density density_ = Encodings::MFM::Density::Single;
		uint8_t first_sector_ = 0;
};

}
