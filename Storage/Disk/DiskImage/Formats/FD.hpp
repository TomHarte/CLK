//
//  FD.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "MFMSectorDump.hpp"

namespace Storage::Disk {

/*!
	Provides a @c Disk containing an FD disk image: a decoded sector dump of a Thomson disk.
*/
class FD: public MFMSectorDump {
public:
	/*!
		Construct an @c FD containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain a .FD format image.
	*/
	FD(const std::string &file_name);

	HeadPosition maximum_head_position() const final;
	int head_count() const final;

private:
	long get_file_offset_for_position(Track::Address) const final;
	int head_count_;
};

}
