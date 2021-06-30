//
//  FAT12.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/01/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#include "FAT12.hpp"

#include "Utility/ImplicitSectors.hpp"

using namespace Storage::Disk;

FAT12::FAT12(const std::string &file_name) :
	MFMSectorDump(file_name) {
	// The only sanity check here is whether a sensible
	// geometry is encoded in the first sector, or can be guessed.
	off_t file_size = file_.stats().st_size;

	if(file_size < 512) throw Error::InvalidFormat;

	// Inspect the FAT.
	file_.seek(11, SEEK_SET);
	sector_size_ = file_.get16le();
	file_.seek(19, SEEK_SET);
	const uint16_t total_sectors = file_.get16le();
	file_.seek(24, SEEK_SET);
	sector_count_ = file_.get16le();
	head_count_ = file_.get16le();

	// Throw if there would seemingly be an incomplete track.
	if(file_size != total_sectors*sector_size_) throw Error::InvalidFormat;
	if(total_sectors % (head_count_ * sector_count_)) throw Error::InvalidFormat;
	track_count_ = int(total_sectors / (head_count_ * sector_count_));

	// Check that there is a valid power-of-two sector size.
	uint8_t log_sector_size = 2;
	while(log_sector_size < 5 && (1 << (7+log_sector_size)) != sector_size_) {
		++log_sector_size;
	}
	if(log_sector_size >= 5) throw Error::InvalidFormat;

	set_geometry(sector_count_, log_sector_size, 1, true);
}

HeadPosition FAT12::get_maximum_head_position() {
	return HeadPosition(track_count_);
}

int FAT12::get_head_count() {
	return head_count_;
}

long FAT12::get_file_offset_for_position(Track::Address address) {
	return (address.position.as_int()*head_count_ + address.head) * sector_size_ * sector_count_;
}
