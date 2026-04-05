//
//  FD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "FD.hpp"

namespace {
// Per https://en.wikipedia.org/wiki/List_of_floppy_disk_formats the Thomson
// uses 16 256-byte sectors per track.
constexpr int sectors_per_track = 16;
constexpr int sector_size = 1;

constexpr int bytes_per_track = sectors_per_track * (128 << sector_size);
}

using namespace Storage::Disk;

FD::FD(const std::string &file_name) : MFMSectorDump(file_name) {
	// Guess disk geometry from file size.
	switch(file_.stats().st_size) {
		default: throw Error::InvalidFormat;

		case bytes_per_track * 40:
			head_count_ = 1;
			track_count_ = 40;
		break;

		case bytes_per_track * 80:
			head_count_ = 1;
			track_count_ = 80;
		break;

		case bytes_per_track * 160:
			head_count_ = 2;
			track_count_ = 80;
		break;
	}

	set_geometry(sectors_per_track, sector_size, 1, Encodings::MFM::Density::Double);
}

HeadPosition FD::maximum_head_position() const {
	return HeadPosition(track_count_);
}

int FD::head_count() const {
	return head_count_;
}

long FD::get_file_offset_for_position(const Track::Address address) const {
	// Determined empirically: disk images are not interleaved.
	return ((address.head * track_count_) + address.position.as_int()) * bytes_per_track;
}
