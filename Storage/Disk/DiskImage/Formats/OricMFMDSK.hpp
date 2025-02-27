//
//  OricMFMDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/11/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides a @c Disk containing an Oric MFM-stype disk image: a stream of the MFM data bits with clocks omitted.
*/
class OricMFMDSK: public DiskImage {
public:
	/*!
		Construct an @c OricMFMDSK containing content from the file with name @c file_name.

		@throws ErrorNotOricMFMDSK if the file doesn't appear to contain an Oric MFM format image.
	*/
	OricMFMDSK(const std::string &file_name);

	// implemented to satisfy @c DiskImage
	HeadPosition maximum_head_position() const;
	int head_count() const;
	bool is_read_only() const;
	bool represents(const std::string &) const;

	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);
	std::unique_ptr<Track> track_at_position(Track::Address) const;

private:
	mutable Storage::FileHolder file_;
	long get_file_offset_for_position(Track::Address address) const;

	uint32_t head_count_;
	uint32_t track_count_;
	uint32_t geometry_type_;
};

}
