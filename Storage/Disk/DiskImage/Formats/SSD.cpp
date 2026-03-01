//
//  SSD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "SSD.hpp"

namespace {
	constexpr int sectors_per_track = 10;
	constexpr int sector_size = 1;
}

using namespace Storage::Disk;

SSD::SSD(const std::string_view file_name) : MFMSectorDump(file_name) {
	// Very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large.

	// Disk has two heads if the suffix is .dsd or if it's too large to be an SSD.
	const bool is_double_sided =
		(tolower(file_name[file_name.size() - 3]) == 'd') ||
		file_.stats().st_size > 80*10*256;

	if(file_.stats().st_size & 255) throw Error::InvalidFormat;
	if(file_.stats().st_size < 512) throw Error::InvalidFormat;
	if(file_.stats().st_size > 80*2*10*256) throw Error::InvalidFormat;

	head_count_ = is_double_sided ? 2 : 1;
	track_count_ = int(file_.stats().st_size / (256 * 10 * head_count_));
	if(track_count_ < 40) track_count_ = 40;
	else if(track_count_ < 80) track_count_ = 80;

	set_geometry(sectors_per_track, sector_size, 0, Encodings::MFM::Density::Single);
}

HeadPosition SSD::maximum_head_position() const {
	return HeadPosition(track_count_);
}

int SSD::head_count() const {
	return head_count_;
}

long SSD::get_file_offset_for_position(const Track::Address address) const {
	return (address.position.as_int() * head_count_ + address.head) * 256 * 10;
}
