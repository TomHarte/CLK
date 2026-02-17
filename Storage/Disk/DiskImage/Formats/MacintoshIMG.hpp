//
//  MacintoshIMG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing either:

		*	a disk imaged by Apple's Disk Copy 4.2: sector contents (optionally plus tag data),
			in either an Apple GCR or standard MFM encoding; or
		*	a raw sector dump of a Macintosh GCR disk.
*/
class MacintoshIMG: public DiskImage {
public:
	/*!
		Construct a @c MacintoshIMG containing content from the file with name @c file_name.

		@throws Error::InvalidFormat if this file doesn't appear to be in Disk Copy 4.2 format.
	*/
	MacintoshIMG(const std::string &file_name);

	enum class FixedType {
		GCR
	};
	/*!
		Constructs a @c MacintoshIMG without attempting to autodetect whether this is a raw
		image or a Disk Copy 4.2 image; if GCR is specified and the file size checks out then
		it is accepted as a GCR image.

		If @c offset and @c length are specified and non-zero, only that portion of the file
		will be modified.
	*/
	MacintoshIMG(std::string_view file_name, FixedType type, size_t offset = 0, size_t length = 0);

	// implemented to satisfy @c Disk
	HeadPosition maximum_head_position() const;
	int head_count() const;
	bool is_read_only() const;
	bool represents(std::string_view) const;

	std::unique_ptr<Track> track_at_position(Track::Address) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);

private:
	mutable Storage::FileHolder file_;

	enum class Encoding {
		GCR400,
		GCR800,
		MFM720,
		MFM1440
	} encoding_;
	uint8_t format_;

	std::vector<uint8_t> data_;
	std::vector<uint8_t> tags_;
	bool is_diskCopy_file_ = false;
	mutable std::mutex buffer_mutex_;

	uint32_t checksum(const std::vector<uint8_t> &, size_t bytes_to_skip = 0) const;
	void construct_raw_gcr(size_t offset, size_t length = 0);
	long raw_offset_ = 0;
};

}
