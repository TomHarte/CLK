//
//  MOOF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"
#include "Storage/FileHolder.hpp"

namespace Storage::Disk {

class MOOF: public DiskImage {
public:
	MOOF(const std::string &file_name);

	HeadPosition maximum_head_position() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
};

}
