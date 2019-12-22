//
//  MSXDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "MSXDSK.hpp"

#include "Utility/ImplicitSectors.hpp"

namespace {
	constexpr int sectors_per_track = 9;
	constexpr int sector_size = 2;
	constexpr off_t track_size = (128 << sector_size)*sectors_per_track;
}

using namespace Storage::Disk;

MSXDSK::MSXDSK(const std::string &file_name) :
	MFMSectorDump(file_name) {
	// The only sanity check here is whether a sensible
	// geometry can be guessed.
	off_t file_size = file_.stats().st_size;

	// Throw if there would seemingly be an incomplete track.
	if(file_size % track_size) throw Error::InvalidFormat;

	track_count_ = static_cast<int>(file_size / track_size);
	head_count_ = 1;

	// Throw if too large or too small or too large for single sided and
	// clearly not double sided.
	if(track_count_ < 40) throw Error::InvalidFormat;
	if(track_count_ > 82*2) throw Error::InvalidFormat;
	if(track_count_ > 82 && track_count_&1) throw Error::InvalidFormat;

	// The below effectively prefers the idea of a single-sided 80-track disk
	// to a double-sided 40-track disk. Emulators have to guess.
	if(track_count_ > 82) {
		track_count_ /= 2;
		head_count_ = 2;
	}

	set_geometry(sectors_per_track, sector_size, 1, true);
}

HeadPosition MSXDSK::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int MSXDSK::get_head_count() {
	return head_count_;
}

long MSXDSK::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int()*head_count_ + address.head) * 512 * 9;
}
