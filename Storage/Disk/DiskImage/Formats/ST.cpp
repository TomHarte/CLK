//
//  ST.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 12/11/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "ST.hpp"

namespace {
	static const int sectors_per_track = 10;
	static const int sector_size = 2;
}

using namespace Storage::Disk;

ST::ST(const std::string &file_name) : MFMSectorDump(file_name) {
	// Very loose validation: the file needs to be a whole number of tracks,
	// and not more than 160 of them.
	const auto stats = file_.stats();
	if(stats.st_size % 512*10) throw Error::InvalidFormat;
	if(stats.st_size > 512*10*160) throw Error::InvalidFormat;

	// Head count: 2 if there are more than 80 tracks. Otherwise 1.
	head_count_ = (stats.st_size >= 512 * 10 * 80) ? 2 : 1;
	track_count_ = std::max(80, int(stats.st_size / (512 * 10 * head_count_)));

	set_geometry(sectors_per_track, sector_size, 1, true);
}

HeadPosition ST::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int ST::get_head_count() {
	return head_count_;
}

long ST::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * head_count_ + address.head) * 512 * sectors_per_track;
}
