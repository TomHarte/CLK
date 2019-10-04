//
//  SSD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "SSD.hpp"

namespace {
	static const int sectors_per_track = 10;
	static const int sector_size = 1;
}

using namespace Storage::Disk;

SSD::SSD(const std::string &file_name) : MFMSectorDump(file_name) {
	// very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large

	if(file_.stats().st_size & 255) throw Error::InvalidFormat;
	if(file_.stats().st_size < 512) throw Error::InvalidFormat;
	if(file_.stats().st_size > 800*256) throw Error::InvalidFormat;

	// this has two heads if the suffix is .dsd, one if it's .ssd
	head_count_ = (tolower(file_name[file_name.size() - 3]) == 'd') ? 2 : 1;
	track_count_ = int(file_.stats().st_size / (256 * 10));
	if(track_count_ < 40) track_count_ = 40;
	else if(track_count_ < 80) track_count_ = 80;

	set_geometry(sectors_per_track, sector_size, 0, false);
}

HeadPosition SSD::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int SSD::get_head_count() {
	return head_count_;
}

long SSD::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * head_count_ + address.head) * 256 * 10;
}
