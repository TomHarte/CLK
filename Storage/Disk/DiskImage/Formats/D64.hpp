//
//  D64.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

#include <filesystem>

namespace Storage::Disk {

/*!
	Provides a @c Disk containing a D64 disk image: a decoded sector dump of a C1540-format disk.
*/
class D64: public DiskImage {
public:
	/*!
		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain a .D64 format image.
	*/
	D64(const std::filesystem::path &);

	HeadPosition maximum_head_position() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool is_read_only() const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &);
	bool represents(const std::filesystem::path &) const;

private:
	mutable Storage::FileHolder file_;
	int number_of_tracks_;
	uint16_t disk_id_;

	struct TrackExtent {
		long file_offset;
		int zone;
		int number_of_sectors;
	};
	TrackExtent track_extent(Track::Address) const;
};

}
