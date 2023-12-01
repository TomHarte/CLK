//
//  FAT12.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "PCBooter.hpp"

#include "Utility/ImplicitSectors.hpp"

using namespace Storage::Disk;

PCBooter::PCBooter(const std::string &file_name) :
	MFMSectorDump(file_name) {
	// The only sanity check here is whether a sensible
	// geometry is encoded in the first sector, or can be guessed.
	const auto file_size = file_.stats().st_size;

	// Check that file size is one of the accepted options.
	switch(file_size) {
		default: throw Error::InvalidFormat;

		// Check for a single-sided, single-density 40-track image.
		case 512 * 9 * 40:
			head_count_ = 1;
			track_count_ = 40;
			set_geometry(9, 2, 1, true);
		break;
	}

	// TODO: check that an appropriate INT or similar is here?
	// Should probably factor out the "does this look like a PC boot sector?" test,
	// as it can also be used to disambiguate FAT12 disks.
}

HeadPosition PCBooter::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int PCBooter::get_head_count() {
	return head_count_;
}

long PCBooter::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * head_count_ + address.head) * 512 * 9;
}
