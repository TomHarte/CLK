//
//  AcornADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "AcornADF.hpp"

#include "Utility/ImplicitSectors.hpp"

namespace {
	constexpr int sectors_per_track = 16;
	constexpr int sector_size = 1;
}

using namespace Storage::Disk;

AcornADF::AcornADF(const std::string &file_name) : MFMSectorDump(file_name) {
	// Check that the disk image contains a whole number of sector.
	if(file_.stats().st_size % off_t(128 << sector_size)) throw Error::InvalidFormat;

	// Check that the disk image is at least large enough to hold an ADFS catalogue.
	if(file_.stats().st_size < 7 * off_t(128 << sector_size)) throw Error::InvalidFormat;

	// Check that the initial directory's 'Hugo's are present.
	file_.seek(513, SEEK_SET);
	uint8_t bytes[4];
	file_.read(bytes, 4);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw Error::InvalidFormat;

	file_.seek(0x6fb, SEEK_SET);
	file_.read(bytes, 4);
	if(bytes[0] != 'H' || bytes[1] != 'u' || bytes[2] != 'g' || bytes[3] != 'o') throw Error::InvalidFormat;

	// Pick a number of heads; treat this image as double sided if it's too large to be single-sided.
	head_count_ = 1 + (file_.stats().st_size > sectors_per_track * off_t(128 << sector_size) * 80);

	// Announce disk geometry.
	set_geometry(sectors_per_track, sector_size, 0, true);
}

HeadPosition AcornADF::get_maximum_head_position() {
	return HeadPosition(80);
}

int AcornADF::get_head_count() {
	return head_count_;
}

long AcornADF::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * head_count_ + address.head) * (128 << sector_size) * sectors_per_track;
}
