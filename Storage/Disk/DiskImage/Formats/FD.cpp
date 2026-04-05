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
constexpr int tracks_per_side = 40;
constexpr int sectors_per_track = 16;
constexpr int sector_size = 1;

constexpr int bytes_per_track = sectors_per_track * (128 << sector_size);
constexpr int bytes_per_side = tracks_per_side * bytes_per_track;
}

using namespace Storage::Disk;

FD::FD(const std::string &file_name) : MFMSectorDump(file_name) {
	// Disk has two heads if the suffix is .dsd or if it's too large to be an SSD.
	const bool is_double_sided = file_.stats().st_size > bytes_per_side;

	if(
		file_.stats().st_size != bytes_per_side &&
		file_.stats().st_size != bytes_per_side * 2
	) throw Error::InvalidFormat;

	head_count_ = is_double_sided ? 2 : 1;

	set_geometry(sectors_per_track, sector_size, 1, Encodings::MFM::Density::Double);
}

HeadPosition FD::maximum_head_position() const {
	return HeadPosition(tracks_per_side);
}

int FD::head_count() const {
	return head_count_;
}

long FD::get_file_offset_for_position(const Track::Address address) const {
	// TODO: RESOLVE GUESS HERE.
	// Guess is that data is interleaved.
	return (address.position.as_int() * head_count_ + address.head) * bytes_per_track;
}
