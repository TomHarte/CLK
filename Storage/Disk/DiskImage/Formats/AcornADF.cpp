//
//  AcornADF.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "AcornADF.hpp"

#include "Utility/ImplicitSectors.hpp"

#include <cstring>

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

	const auto has_identifier = [&](long location, bool permit_hugo, bool permit_nick) -> bool {
		file_.seek(location, SEEK_SET);

		uint8_t bytes[4];
		file_.read(bytes, 4);

		return
			(permit_hugo && !memcmp(bytes, "Hugo", 4)) ||
			(permit_nick && !memcmp(bytes, "Nick", 4));
	};

	const auto file_size = file_.stats().st_size;
	Encodings::MFM::Density density = Encodings::MFM::Density::Double;
	if(has_identifier(513, true, false)) {
		// One of:
		//
		// ADFS-S: 1 side, 40 tracks, 16 sectors, 256 bytes = 160K old map, old dir
		// ADFS-M: 1 side, 80 tracks, 16 sectors, 256 bytes = 320K old map, old dir
		// ADFS-L: 2 sides, 80 tracks, 16 sectors, 256 bytes = 640K old map, old dir

		head_count_ = file_.stats().st_size > 80*16*256 ? 2 : 1;
		sector_size_ = 1;
		sectors_per_track_ = 16;
	} else if(has_identifier(1025, true, true)) {
		// ADFS-D: 80 tracks, 2 sides, 5 sectors, 1024 bytes = 800K old map, new dir
		head_count_ = 2;
		sector_size_ = 3;
		sectors_per_track_ = 5;
	} else if(has_identifier(2049, false, true)) {
		// One of:
		//
		// ADFS-E: 80 tracks, 2 sides, 5 sectors, 1024 bytes = 800K new map, new dir
		// ADFS-F: 80 tracks, 2 sides, 10 sectors, 1024 bytes = 1600K new map, new dir
		// ADFS-G: 80 tracks, 2 sides, 20 sectors, 1024 bytes = 3200K new map, new dir

		head_count_ = 2;
		sector_size_ = 3;
		if(file_size > 80*2*10*1024) {
			sectors_per_track_ = 20;
			density = Encodings::MFM::Density::High;	// Or, presumably, higher than high?
		} else if(file_size > 80*2*10*1024) {
			sectors_per_track_ = 10;
			density = Encodings::MFM::Density::High;
		} else {
			sectors_per_track_ = 5;
		}
	} else {
		throw Error::InvalidFormat;
	}

	// TODO: possibly this image is side-interleaved if sectors per track is less than 16.
	// Figure that out.

	// Check that the disk image is at least large enough to hold an ADFS catalogue.
	if(file_.stats().st_size < 7 * sizeT(128 << sector_size_)) throw Error::InvalidFormat;

	// Announce disk geometry.
	set_geometry(sectors_per_track_, sector_size_, 0, density);
}

HeadPosition AcornADF::get_maximum_head_position() const {
	return HeadPosition(80);
}

int AcornADF::get_head_count() const {
	return head_count_;
}

long AcornADF::get_file_offset_for_position(Track::Address address) const {
	return (address.position.as_int() * head_count_ + address.head) * (128 << sector_size_) * sectors_per_track_;
}
