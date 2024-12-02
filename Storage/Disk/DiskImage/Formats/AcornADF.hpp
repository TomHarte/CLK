//
//  AcornADF.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "MFMSectorDump.hpp"

#include <string>

namespace Storage::Disk {

/*!
	Provides a @c Disk containing an ADF disk image: a decoded sector dump of an Acorn ADFS disk.
*/
class AcornADF: public MFMSectorDump {
public:
	/*!
		Construct an @c AcornADF containing content from the file with name @c file_name.

		@throws Storage::FileHolder::Error::CantOpen if this file can't be opened.
		@throws Error::InvalidFormat if the file doesn't appear to contain an Acorn .ADF format image.
	*/
	AcornADF(const std::string &file_name);

	HeadPosition get_maximum_head_position() final;
	int get_head_count() final;

private:
	long get_file_offset_for_position(Track::Address) final;
	int head_count_ = 1;
	uint8_t sector_size_ = 1;
	int sectors_per_track_ = 16;
};

}
