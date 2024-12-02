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
	HeadPosition get_maximum_head_position() final;
	int get_head_count() final;
	std::shared_ptr<::Storage::Disk::Track> get_track_at_position(::Storage::Disk::Track::Address address) final;

private:
	FileHolder file_;
	std::map<Storage::Disk::Track::Address, long> track_locations_;
	uint8_t cylinders_ = 0, heads_ = 0;
};

}
