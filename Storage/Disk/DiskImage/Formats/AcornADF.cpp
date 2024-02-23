//
//  AcornADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "AcornADF.hpp"

#include "Utility/ImplicitSectors.hpp"

using namespace Storage::Disk;

AcornADF::AcornADF(const std::string &file_name) : MFMSectorDump(file_name) {
	// Check that the disk image contains a whole number of sector.
	using sizeT = decltype(file_.stats().st_size);
	const auto size = file_.stats().st_size;

	if(size < 1024) throw Error::InvalidFormat;

	// Definitely true: a directory signature of 'Hugo' can be read by both 8-bit
	// machines and the Archimedes. 'Nick' can be read only by the Archimedes.
	//
	// https://mdfs.net/Docs/Comp/Disk/Format/ADFS then falsely states that:
	//
	//		The type of ADFS filesystem can be determined by looking for the "Hugo"/
	//		"Nick" identifier that marks the start of the root directory 512 bytes into
	//		the filesystem and 1024 bytes in.
	//
	// In terms of .ADF files:
	//
	//	all 8-bit files seem to have 'Hugo' at offset 513;
	//	ADFS-D (early Arc, late BBC Master) has 'Nick' or 'Hugo' at 1025; but
	//	ADFS-E (most Arc) has 'Hugo' at 2049.
	//
	// Even allowing for the document having failed to account for the directory ID,
	// I can't reconcile that 2049 offset with being 1024 bytes into the file system.
	//
	// That document claims that ADFS-D and ADFS-E are logically interleaved but
	// https://github.com/android444/fluxengine/blob/master/doc/disk-acornadfs.md
	// states that:
	//
	//		Acorn logical block numbering goes all the way up side 0 and then all the
	//		way up side 1. However, FluxEngine uses traditional disk images with alternating
	//		sides, with the blocks from track 0 side 0 then track 0 side 1 then track 1 side 0 etc.
	//		Most Acorn emulators will use both formats, but they might require nudging as the side
	//		order can't be reliably autodetected.
	//
	// So then .ADF files might be track-interleaved and might not be.

	switch(size) {
		default:
			sector_size_ = 1;
			sectors_per_track_ = 16;
		break;
	}

	// Check that the disk image is at least large enough to hold an ADFS catalogue.
	if(file_.stats().st_size < 7 * sizeT(128 << sector_size_)) throw Error::InvalidFormat;

	// Check that the initial directory's 'Hugo's or 'Nick's are present.
	// TODO: check other locations, to pick sector size and count.
	file_.seek(513, SEEK_SET);
	uint8_t bytes[4];
	file_.read(bytes, 4);
	if(memcmp(bytes, "Hugo", 4) && memcmp(bytes, "Nick", 4)) throw Error::InvalidFormat;

	file_.seek(0x6fb, SEEK_SET);
	file_.read(bytes, 4);
	if(memcmp(bytes, "Hugo", 4) && memcmp(bytes, "Nick", 4)) throw Error::InvalidFormat;

	// Pick a number of heads; treat this image as double sided if it's too large to be single-sided.
	head_count_ = 1 + (file_.stats().st_size > sectors_per_track_ * sizeT(128 << sector_size_) * 80);

	// Announce disk geometry.
	set_geometry(sectors_per_track_, sector_size_, 0, Encodings::MFM::Density::Double);
}

HeadPosition AcornADF::get_maximum_head_position() {
	return HeadPosition(80);
}

int AcornADF::get_head_count() {
	return head_count_;
}

long AcornADF::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int() * head_count_ + address.head) * (128 << sector_size_) * sectors_per_track_;
}
