//
//  AmigaADF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "MFMSectorDump.hpp"

#include <filesystem>
#include <string>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing an Amiga ADF, which is an MFM sector contents dump,
	but the Amiga doesn't use IBM-style sector demarcation.
*/
class AmigaADF: public DiskImage {
public:
	/*!
		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an .ADF format image.
	*/
	AmigaADF(const std::filesystem::path &);

	// implemented to satisfy @c Disk
	HeadPosition maximum_head_position() const;
	int head_count() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool represents(const std::filesystem::path &) const;

private:
	mutable Storage::FileHolder file_;
	long get_file_offset_for_position(Track::Address) const;

};

}
