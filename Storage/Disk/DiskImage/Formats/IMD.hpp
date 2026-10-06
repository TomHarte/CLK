//
//  IMD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

#include <filesystem>

namespace Storage::Disk {

/*!
	Provides an @c DiskImage containing an IMD image, which is a collection of arbitrarily-numbered FM or MFM
	sectors collected by track.
*/

class IMD: public DiskImage {
public:
	/*!
		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an Acorn .ADF format image.
	*/
	IMD(const std::filesystem::path &);

	// DiskImage interface.
	HeadPosition maximum_head_position() const;
	int head_count() const;
	bool represents(const std::filesystem::path &) const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;

private:
	mutable FileHolder file_;
	std::map<Storage::Disk::Track::Address, long> track_locations_;
	uint8_t cylinders_ = 0, heads_ = 0;
};

}
