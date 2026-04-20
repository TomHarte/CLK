//
//  SAP.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Storage/FileHolder.hpp"
#include <string>

namespace Storage::Disk {

class SAP: public DiskImage {
public:
	SAP(const std::string &file_name);

	HeadPosition maximum_head_position() const;
	Track::Address canonical_address(Track::Address) const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	void set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks);
	bool is_read_only() const;
	bool represents(const std::string &) const;

private:
	mutable FileHolder file_;
	uint8_t sector_size_;
};

}
