//
//  HFE.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

#include <cstdint>
#include <map>
#include <string_view>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing an HFE: a bit stream representation of a floppy.
*/
class HFE: public DiskImage {
public:
	/*!
		Construct an @c HFE containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an .HFE format image.
		@throws Error::UnknownVersion if the file looks correct but is an unsupported version.
	*/
	HFE(std::string_view file_name);

	// implemented to satisfy @c Disk
	HeadPosition maximum_head_position() const;
	int head_count() const;
	bool is_read_only() const;
	bool represents(std::string_view) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);
	std::unique_ptr<Track> track_at_position(Track::Address) const;

private:
	mutable Storage::FileHolder file_;
	uint16_t seek_track(Track::Address address) const;

	int head_count_;
	int track_count_;
	long track_list_offset_;
};

}
