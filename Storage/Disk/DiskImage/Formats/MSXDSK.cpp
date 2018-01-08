//
//  MSXDSK.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#include "MSXDSK.hpp"

#include "Utility/ImplicitSectors.hpp"

namespace {
	static const int sectors_per_track = 9;
	static const int sector_size = 2;
}

using namespace Storage::Disk;

MSXDSK::MSXDSK(const char *file_name) :
	MFMSectorDump(file_name) {
	// The only sanity check here is whether a sensible
	// geometry can be guessed.
	off_t file_size = file_.stats().st_size;
	const off_t track_size = 512*9;

	// Throw if there would seemingly be an incomplete track.
	if(file_size % track_size) throw ErrorNotMSXDSK;

	track_count_ = static_cast<int>(file_size / track_size);
	head_count_ = 1;

	// Throw if too large or too small or too large for single sided and
	// clearly not double sided.
	if(track_count_ < 40) throw ErrorNotMSXDSK;
	if(track_count_ > 82*2) throw ErrorNotMSXDSK;
	if(track_count_ > 82 && track_count_&1) throw ErrorNotMSXDSK;

	// The below effectively prefers the idea of a single-sided 80-track disk
	// to a double-sided 40-track disk. Emulators have to guess.
	if(track_count_ > 82) {
		track_count_ /= 2;
		head_count_ = 2;
	}

	set_geometry(sectors_per_track, sector_size, 1, true);
}

int MSXDSK::get_head_position_count() {
	return track_count_;
}

int MSXDSK::get_head_count() {
	return head_count_;
}

long MSXDSK::get_file_offset_for_position(Track::Address address) {
	return (address.position*2 + address.head) * 512 * 9;
}
