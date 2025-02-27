//
//  AppleDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing an Apple DSK disk image: a representation of sector contents,
	implicitly numbered and located.
*/
class AppleDSK: public DiskImage {
public:
	/*!
		Construct an @c AppleDSK containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an Apple DSK format image.
	*/
	AppleDSK(const std::string &file_name);

	// Implemented to satisfy @c DiskImage.
	HeadPosition maximum_head_position() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &);
	bool is_read_only() const;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
	int sectors_per_track_ = 16;
	bool is_prodos_ = false;

	long file_offset(Track::Address) const;
	size_t logical_sector_for_physical_sector(size_t physical) const;
};

}
