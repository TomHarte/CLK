//
//  JFD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/05/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/Disk/DiskImage/DiskImage.hpp"

#include <string>
#include <zlib.h>

namespace Storage::Disk {

class JFD: public DiskImage {
public:
	JFD(const std::string &file_name);

	HeadPosition maximum_head_position() const;
	int head_count() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool represents(const std::string &) const;

private:
	std::string file_name_;
	gzFile file_;
	uint8_t read8() const;
	uint32_t read32() const;

	uint32_t track_offset_;
	uint32_t sector_offset_;
	uint32_t data_offset_;
};

}
