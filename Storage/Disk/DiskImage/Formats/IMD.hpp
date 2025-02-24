//
//  IMD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage::Disk {

/*!
	Provides an @c DiskImage containing an IMD image, which is a collection of arbitrarily-numbered FM or MFM
	sectors collected by track.
*/

class IMD: public DiskImage {
public:
	/*!
		Construct an @c IMD containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an Acorn .ADF format image.
	*/
	IMD(const std::string &file_name);

	// DiskImage interface.
	HeadPosition get_maximum_head_position() const;
	int get_head_count() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;

private:
	mutable FileHolder file_;
	std::map<Storage::Disk::Track::Address, long> track_locations_;
	uint8_t cylinders_ = 0, heads_ = 0;
};

}
