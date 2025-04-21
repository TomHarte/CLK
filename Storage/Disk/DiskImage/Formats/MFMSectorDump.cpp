//
//  MFMSectorDump.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "MFMSectorDump.hpp"

#include "Storage/Disk/DiskImage/Formats/Utility/ImplicitSectors.hpp"

using namespace Storage::Disk;

MFMSectorDump::MFMSectorDump(const std::string &file_name) : file_(file_name) {}

void MFMSectorDump::set_geometry(int sectors_per_track, uint8_t sector_size, uint8_t first_sector, Encodings::MFM::Density density) {
	sectors_per_track_ = sectors_per_track;
	sector_size_ = sector_size;
	density_ = density;
	first_sector_ = first_sector;
}

std::unique_ptr<Track> MFMSectorDump::track_at_position(Track::Address address) const {
	if(address.head >= head_count()) return nullptr;
	if(address.position.as_largest() >= maximum_head_position().as_largest()) return nullptr;

	const auto size = size_t((128 << sector_size_) * sectors_per_track_);
	std::vector<uint8_t> sectors;
	const long file_offset = get_file_offset_for_position(address);

	{
		std::lock_guard lock_guard(file_.file_access_mutex());
		file_.seek(file_offset, SEEK_SET);
		sectors = file_.read(size);
	}

	return track_for_sectors(
		sectors.data(),
		sectors_per_track_,
		uint8_t(address.position.as_int()),
		uint8_t(address.head),
		first_sector_,
		sector_size_,
		density_);
}

void MFMSectorDump::set_tracks(const std::map<Track::Address, std::unique_ptr<Track>> &tracks) {
	const auto size = size_t((128 << sector_size_) * sectors_per_track_);
	std::vector<uint8_t> parsed_track(size);

	// TODO: it would be more efficient from a file access and locking point of view to parse the sectors
	// in one loop, then write in another.

	for(const auto &track : tracks) {
		decode_sectors(
			*track.second,
			parsed_track.data(),
			first_sector_,
			first_sector_ + uint8_t(sectors_per_track_-1),
			sector_size_,
			density_);
		const long file_offset = get_file_offset_for_position(track.first);

		std::lock_guard lock_guard(file_.file_access_mutex());
		file_.ensure_is_at_least_length(file_offset);
		file_.seek(file_offset, SEEK_SET);
		file_.write(parsed_track);
	}
	file_.flush();
}

bool MFMSectorDump::is_read_only() const {
	return file_.is_known_read_only();
}

bool MFMSectorDump::represents(const std::string &name) const {
	return name == file_.name();
}
