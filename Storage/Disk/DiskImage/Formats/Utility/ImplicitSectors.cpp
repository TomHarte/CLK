//
//  ImplicitSectors.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "ImplicitSectors.hpp"

#include "../../../Encodings/MFM/Sector.hpp"
#include "../../../Encodings/MFM/Encoder.hpp"
#include "../../../Encodings/MFM/Constants.hpp"
#include "../../../Track/TrackSerialiser.hpp"
#include "../../../Encodings/MFM/SegmentParser.hpp"

using namespace Storage::Disk;

std::shared_ptr<Track> Storage::Disk::track_for_sectors(uint8_t *const source, uint8_t track, uint8_t side, uint8_t first_sector, uint8_t size, bool is_double_density) {
	std::vector<Storage::Encodings::MFM::Sector> sectors;

	off_t byte_size = static_cast<off_t>(128 << size);
	off_t source_pointer = 0;
	for(int sector = 0; sector < 10; sector++) {
		sectors.emplace_back();

		Storage::Encodings::MFM::Sector &new_sector = sectors.back();
		new_sector.address.track = track;
		new_sector.address.side = size;
		new_sector.address.sector = first_sector;
		first_sector++;
		new_sector.size = size;

		new_sector.data.insert(new_sector.data.begin(), source + source_pointer, source + source_pointer + byte_size);
		source_pointer += byte_size;
	}

	if(sectors.size()) {
		return is_double_density ? Storage::Encodings::MFM::GetMFMTrackWithSectors(sectors) : Storage::Encodings::MFM::GetFMTrackWithSectors(sectors);
	}

	return nullptr;
}

void Storage::Disk::decode_sectors(Track &track, uint8_t *const destination, uint8_t first_sector, uint8_t last_sector, uint8_t sector_size, bool is_double_density) {
	std::map<size_t, Storage::Encodings::MFM::Sector> sectors =
		Storage::Encodings::MFM::sectors_from_segment(
			Storage::Disk::track_serialisation(track, is_double_density ? Storage::Encodings::MFM::MFMBitLength : Storage::Encodings::MFM::FMBitLength),
			is_double_density);

	size_t byte_size = static_cast<size_t>(128 << sector_size);
	for(auto &pair : sectors) {
		if(pair.second.address.sector > last_sector) continue;
		if(pair.second.address.sector < first_sector) continue;
		if(pair.second.size != sector_size) continue;
		memcpy(&destination[pair.second.address.sector * byte_size], pair.second.data.data(), std::min(pair.second.data.size(), byte_size));
	}
}
