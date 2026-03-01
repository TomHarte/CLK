//
//  DMK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

#include <string_view>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing a DMK disk image: mostly a decoded byte stream, but with
	a record of IDAM locations.
*/
class DMK: public DiskImage {
public:
	/*!
		Construct a @c DMK containing content from the file with name @c file_name.

		@throws Error::InvalidFormat if this file doesn't appear to be a DMK.
	*/
	DMK(std::string_view file_name);

	HeadPosition maximum_head_position() const;
	int head_count() const;
	bool is_read_only() const;
	bool represents(std::string_view) const;

	std::unique_ptr<Track> track_at_position(Track::Address) const;

private:
	mutable FileHolder file_;
	long get_file_offset_for_position(Track::Address address) const;

	bool is_read_only_;
	int head_position_count_;
	int head_count_;

	long track_length_;
	bool is_purely_single_density_;
};

}
