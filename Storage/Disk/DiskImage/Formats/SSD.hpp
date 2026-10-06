//
//  SSD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "MFMSectorDump.hpp"

#include <filesystem>

namespace Storage::Disk {

/*!
	Provides a @c Disk containing a DSD or SSD disk image: a decoded sector dump of an Acorn DFS disk.
*/
class SSD: public MFMSectorDump {
public:
	/*!
		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain a .SSD format image.
	*/
	SSD(const std::filesystem::path &);

	HeadPosition maximum_head_position() const final;
	int head_count() const final;

private:
	long get_file_offset_for_position(Track::Address) const final;

	int head_count_;
	int track_count_;
};

}
