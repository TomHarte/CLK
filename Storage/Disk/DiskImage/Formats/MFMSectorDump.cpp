//
//  MFMSectorDump.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "MFMSectorDump.hpp"

#include "Utility/ImplicitSectors.hpp"

using namespace Storage::Disk;

MFMSectorDump::MFMSectorDump(const char *file_name) : Storage::FileHolder(file_name) {}

void MFMSectorDump::set_geometry(int sectors_per_track, uint8_t sector_size, bool is_double_density) {
	sectors_per_track_ = sectors_per_track;
	sector_size_ = sector_size;
	is_double_density_ = is_double_density;
}

std::shared_ptr<Track> MFMSectorDump::get_track_at_position(unsigned int head, unsigned int position) {
	uint8_t sectors[(128 << sector_size_)*sectors_per_track_];

	if(head > 1) return nullptr;
	long file_offset = get_file_offset_for_position(head, position);

	{
		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		fseek(file_, file_offset, SEEK_SET);
		fread(sectors, 1, sizeof(sectors), file_);
	}

	return track_for_sectors(sectors, (uint8_t)position, (uint8_t)head, 0, sector_size_, is_double_density_);
}

void MFMSectorDump::set_track_at_position(unsigned int head, unsigned int position, const std::shared_ptr<Track> &track) {
	uint8_t parsed_track[(128 << sector_size_)*sectors_per_track_];
	// Assumption here: sector IDs will run from 0.
	decode_sectors(*track, parsed_track, 0, (uint8_t)(sectors_per_track_-1), sector_size_, is_double_density_);

	long file_offset = get_file_offset_for_position(head, position);

	std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
	ensure_file_is_at_least_length(file_offset);
	fseek(file_, file_offset, SEEK_SET);
	fwrite(parsed_track, 1, sizeof(parsed_track), file_);
	fflush(file_);
}
