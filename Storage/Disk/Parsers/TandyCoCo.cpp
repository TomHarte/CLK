//
//  TandyCoCo.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "TandyCoCo.hpp"

#include "Storage/Disk/Encodings/MFM/Parser.hpp"

using namespace Storage::Disk;

bool TandyCoCo::has_boot_track(const Storage::Disk::Disk &disk) {
	Storage::Encodings::MFM::Parser parser(Storage::Encodings::MFM::Density::Double, disk);
	const Storage::Encodings::MFM::Sector *sector = parser.sector(0, 34, 1);
	if(!sector) return false;

	const auto &body = sector->samples.front();
	return body[0] == 'O' && body[1] == 'S';
}

std::optional<std::vector<TandyCoCo::DirectoryEntry>> TandyCoCo::directory(const Storage::Disk::Disk &disk) {
	Storage::Encodings::MFM::Parser parser(Storage::Encodings::MFM::Density::Double, disk);
	std::vector<TandyCoCo::DirectoryEntry> entries;

	const Storage::Encodings::MFM::Sector *sector = nullptr;
	uint8_t sector_number = 3;
	size_t entry_pointer = 0;
	while(sector_number < 12) {
		if(!sector) {
			sector = parser.sector(0, 17, sector_number);
			entry_pointer = 0;
			if(!sector) return std::nullopt;
		}

		const uint8_t *const entry = &sector->samples.front()[entry_pointer];

		// "If byte0 is $FF, then the entry and all following entries have never been used."
		if(entry[0] == 0xff) {
			break;
		}

		// If byte0 is 0, then the file has been ‘KILL’ed and the directory entry is available for use."
		if(entry[0] != 0) {
			auto &file = entries.emplace_back();
			file.name = std::string(&entry[0], &entry[8]);
			file.extension = std::string(&entry[8], &entry[11]);
			file.file_type = TandyCoCo::DirectoryEntry::FileType(entry[11]);
			file.ascii_flag = TandyCoCo::DirectoryEntry::ASCIIFlag(entry[12]);
			file.first_granule = entry[13];
			file.bytes_in_last_granule = uint16_t((entry[14] << 8) | entry[15]);
		}

		entry_pointer += 32;
		if(entry_pointer == 256) {
			sector = nullptr;
			++sector_number;
		}
	}

	return entries;
}
