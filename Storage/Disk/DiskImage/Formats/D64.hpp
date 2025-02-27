//
//  D64.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

namespace Storage::Disk {

/*!
	Provides a @c Disk containing a D64 disk image: a decoded sector dump of a C1540-format disk.
*/
class D64: public DiskImage {
public:
	/*!
		Construct a @c D64 containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain a .D64 format image.
	*/
	D64(const std::string &file_name);

	HeadPosition maximum_head_position() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
	int number_of_tracks_;
	uint16_t disk_id_;
};

}
