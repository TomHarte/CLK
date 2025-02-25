//
//  NIB.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"
#include "../../Track/PCMTrack.hpp"

#include <memory>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage describing an Apple NIB disk image:
	a bit stream capture that omits sync zeroes, and doesn't define
	the means for full reconstruction.
*/
class NIB: public DiskImage {
public:
	NIB(const std::string &file_name);

	// Implemented to satisfy @c DiskImage.
	HeadPosition get_maximum_head_position() const;
	Track::Address canonical_address(Track::Address) const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);
	bool get_is_read_only() const;
	bool represents(const std::string &) const;

private:
	mutable FileHolder file_;
	long get_file_offset_for_position(Track::Address address) const;
	long file_offset(Track::Address address) const;
};

}
