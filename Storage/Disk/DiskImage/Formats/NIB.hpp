//
//  NIB.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Storage/FileHolder.hpp"

#include <string_view>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage describing an Apple NIB disk image:
	a bit stream capture that omits sync zeroes, and doesn't define
	the means for full reconstruction.
*/
class NIB: public DiskImage {
public:
	NIB(std::string_view file_name);

	// Implemented to satisfy @c DiskImage.
	HeadPosition maximum_head_position() const;
	Track::Address canonical_address(Track::Address) const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);
	bool is_read_only() const;
	bool represents(std::string_view) const;

private:
	mutable FileHolder file_;
	long get_file_offset_for_position(Track::Address address) const;
	long file_offset(Track::Address address) const;
};

}
