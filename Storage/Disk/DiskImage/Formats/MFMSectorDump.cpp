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

std::shared_ptr<Track> MFMSectorDump::get_track_at_position(Track::Address address) {
	uint8_t sectors[(128 << sector_size_)*sectors_per_track_];

	if(address.head > 1) return nullptr;
	long file_offset = get_file_offset_for_position(address);

	{
		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		fseek(file_, file_offset, SEEK_SET);
		fread(sectors, 1, sizeof(sectors), file_);
	}

	return track_for_sectors(sectors, static_cast<uint8_t>(address.position), static_cast<uint8_t>(address.head), 0, sector_size_, is_double_density_);
}

void MFMSectorDump::set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) {
	uint8_t parsed_track[(128 << sector_size_)*sectors_per_track_];

	// TODO: it would be more efficient from a file access and locking point of view to parse the sectors
	// in one loop, then write in another.

	for(auto &track : tracks) {
		// Assumption here: sector IDs will run from 0.
		decode_sectors(*track.second, parsed_track, 0, static_cast<uint8_t>(sectors_per_track_-1), sector_size_, is_double_density_);
		long file_offset = get_file_offset_for_position(track.first);

		std::lock_guard<std::mutex> lock_guard(file_access_mutex_);
		ensure_file_is_at_least_length(file_offset);
		fseek(file_, file_offset, SEEK_SET);
		fwrite(parsed_track, 1, sizeof(parsed_track), file_);
	}
	fflush(file_);
}
