//
//  G64.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/07/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides a @c Disk containing a G64 disk image: a raw but perfectly-clocked GCR stream.
*/
class G64: public DiskImage {
public:
	/*!
		Construct a @c G64 containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain a .G64 format image.
		@throws Error::UnknownVersion if this file appears to be a .G64 but has an unrecognised version number.
	*/
	G64(const std::string &file_name);

	// implemented to satisfy @c Disk
	HeadPosition maximum_head_position() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	using DiskImage::is_read_only;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
	uint8_t number_of_tracks_;
	uint16_t maximum_track_size_;
};

}
