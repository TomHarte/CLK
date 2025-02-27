//
//  WOZ.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"
#include "../../../../Numeric/CRC.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing a WOZ: a bit stream representation of a floppy.
*/
class WOZ: public DiskImage {
public:
	WOZ(const std::string &file_name);

	// Implemented to satisfy @c DiskImage.
	HeadPosition maximum_head_position() const;
	int head_count() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);
	bool is_read_only() const;
	bool tracks_differ(Track::Address, Track::Address) const;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
	enum class Type {
		WOZ1, WOZ2
	} type_ = Type::WOZ1;
	bool is_read_only_ = false;
	bool is_3_5_disk_ = false;
	uint8_t track_map_[160];
	long tracks_offset_ = -1;

	std::vector<uint8_t> post_crc_contents_;

	/*!
		Gets the in-file offset of a track.

		@returns The offset within the file of the track at @c address or @c NoSuchTrack if
			the track does not exit.
	*/
	long file_offset(Track::Address address) const;
	constexpr static long NoSuchTrack = 0;	// This is an offset a track definitely can't lie at.
};

}
