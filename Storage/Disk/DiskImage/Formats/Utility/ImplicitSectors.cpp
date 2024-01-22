//
//  ImplicitSectors.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 29/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "ImplicitSectors.hpp"

#include <cstring>

#include "../../../Encodings/MFM/Sector.hpp"
#include "../../../Encodings/MFM/Encoder.hpp"
#include "../../../Encodings/MFM/Constants.hpp"
#include "../../../Track/TrackSerialiser.hpp"
#include "../../../Encodings/MFM/SegmentParser.hpp"

using namespace Storage::Disk;

std::shared_ptr<Track> Storage::Disk::track_for_sectors(
	const uint8_t *const source,
	int number_of_sectors,
	uint8_t track,
	uint8_t side,
	uint8_t first_sector,
	uint8_t size,
	Storage::Encodings::MFM::Density density
) {
	std::vector<Storage::Encodings::MFM::Sector> sectors;

	size_t byte_size = size_t(128 << size);
	size_t source_pointer = 0;
	for(int sector = 0; sector < number_of_sectors; sector++) {
		sectors.emplace_back();

		Storage::Encodings::MFM::Sector &new_sector = sectors.back();
		new_sector.address.track = track;
		new_sector.address.side = side;
		new_sector.address.sector = first_sector;
		first_sector++;
		new_sector.size = size;

		new_sector.samples.emplace_back();
		new_sector.samples[0].insert(new_sector.samples[0].begin(), source + source_pointer, source + source_pointer + byte_size);
		source_pointer += byte_size;
	}

	if(!sectors.empty()) {
		return TrackWithSectors(density, sectors);
	}

	return nullptr;
}

void Storage::Disk::decode_sectors(
	const Track &track,
	uint8_t *const destination,
	uint8_t first_sector,
	uint8_t last_sector,
	uint8_t sector_size,
	Storage::Encodings::MFM::Density density
) {
	std::map<std::size_t, Storage::Encodings::MFM::Sector> sectors =
		Storage::Encodings::MFM::sectors_from_segment(
			Storage::Disk::track_serialisation(
				track,
				Storage::Encodings::MFM::bit_length(density)
			),
			density);

	std::size_t byte_size = size_t(128 << sector_size);
	for(const auto &pair : sectors) {
		if(pair.second.address.sector > last_sector) continue;
		if(pair.second.address.sector < first_sector) continue;
		if(pair.second.size != sector_size) continue;
		if(pair.second.samples.empty()) continue;
		std::memcpy(&destination[(pair.second.address.sector - first_sector) * byte_size], pair.second.samples[0].data(), std::min(pair.second.samples[0].size(), byte_size));
	}
}
