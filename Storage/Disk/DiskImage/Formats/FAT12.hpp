//
//  FAT12.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "MFMSectorDump.hpp"

#include <filesystem>
#include <string>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage holding an MSDOS-style FAT12 disk image:
	a sector dump of appropriate proportions.
*/
class FAT12: public MFMSectorDump {
public:
	FAT12(const std::filesystem::path &);
	HeadPosition maximum_head_position() const final;
	int head_count() const final;

private:
	long get_file_offset_for_position(Track::Address) const;

	int head_count_;
	int track_count_;
	int sector_count_;
	int sector_size_;
};

}
