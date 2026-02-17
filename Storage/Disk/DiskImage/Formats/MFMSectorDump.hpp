//
//  MFMSectorDump.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"
#include "Storage/Disk/Encodings/MFM/Constants.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides the base for writeable [M]FM disk images that just contain contiguous sector content dumps.
*/
class MFMSectorDump: public DiskImage {
public:
	MFMSectorDump(std::string_view file_name);

	bool is_read_only() const;
	bool represents(std::string_view) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);
	std::unique_ptr<Track> track_at_position(Track::Address) const;

protected:
	mutable Storage::FileHolder file_;
	void set_geometry(int sectors_per_track, uint8_t sector_size, uint8_t first_sector, Encodings::MFM::Density);

private:
	virtual int head_count() const = 0;
	virtual HeadPosition maximum_head_position() const = 0;
	virtual long get_file_offset_for_position(Track::Address) const = 0;

	int sectors_per_track_ = 0;
	uint8_t sector_size_ = 0;
	Encodings::MFM::Density density_ = Encodings::MFM::Density::Single;
	uint8_t first_sector_ = 0;
};

}
