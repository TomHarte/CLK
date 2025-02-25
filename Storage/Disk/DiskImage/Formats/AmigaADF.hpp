//
//  AmigaADF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "MFMSectorDump.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides a @c DiskImage containing an Amiga ADF, which is an MFM sector contents dump,
	but the Amiga doesn't use IBM-style sector demarcation.
*/
class AmigaADF: public DiskImage {
public:
	/*!
		Construct an @c AmigaADF containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an .ADF format image.
	*/
	AmigaADF(const std::string &file_name);

	// implemented to satisfy @c Disk
	HeadPosition get_maximum_head_position() const;
	int get_head_count() const;
	std::unique_ptr<Track> track_at_position(Track::Address) const;
	bool represents(const std::string &) const;

private:
	mutable Storage::FileHolder file_;
	long get_file_offset_for_position(Track::Address) const;

};

}
