//
//  FAT.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "FAT.hpp"

#include "../Encodings/MFM/Parser.hpp"

#include <iostream>

using namespace Storage::Disk;

FAT::Volume::CHS FAT::Volume::chs_for_sector(int sector) const {
	const auto track = sector / sectors_per_track;

	// Sides are interleaved.
	return CHS{
		track / head_count,
		track % head_count,
		1 + (sector % sectors_per_track)
	 };
}

int FAT::Volume::sector_for_cluster(uint16_t cluster) const {
	// The first cluster in the data area is numbered as 2.
	return ((cluster - 2) * sectors_per_cluster) + first_data_sector;
}

namespace {

FAT::Directory directory_from(const std::vector<uint8_t> &contents) {
	FAT::Directory result;

	// Worst case: parse until the amount of data supplied is fully consumed.
	for(size_t base = 0; base < contents.size(); base += 32) {
		// An entry starting with byte 0 indicates end-of-directory.
		if(!contents[base]) {
			break;
		}

		// An entry starting in 0xe5 is merely deleted.
		if(contents[base] == 0xe5) {
			continue;
		}

		// Otherwise create and populate a new entry.
		result.emplace_back();
		result.back().name = std::string(&contents[base], &contents[base+8]);
		result.back().extension = std::string(&contents[base+8], &contents[base+11]);
		result.back().attributes = contents[base + 11];
		result.back().time = uint16_t(contents[base+22] | (contents[base+23] << 8));
		result.back().date = uint16_t(contents[base+24] | (contents[base+25] << 8));
		result.back().starting_cluster = uint16_t(contents[base+26] | (contents[base+27] << 8));
		result.back().size = uint32_t(
			contents[base+28] |
			(contents[base+29] << 8) |
			(contents[base+30] << 16) |
			(contents[base+31] << 24)
		);
	}

	return result;
}

}

std::optional<FAT::Volume> FAT::GetVolume(const std::shared_ptr<Storage::Disk::Disk> &disk) {
	Storage::Encodings::MFM::Parser parser(true, disk);

	// Grab the boot sector; that'll be enough to establish the volume.
	Storage::Encodings::MFM::Sector *const boot_sector = parser.get_sector(0, 0, 1);
	if(!boot_sector || boot_sector->samples.empty() || boot_sector->samples[0].size() < 512) {
		return std::nullopt;
	}

	// Obtain volume details.
	const auto &data = boot_sector->samples[0];
	FAT::Volume volume;
	volume.bytes_per_sector = uint16_t(data[11] | (data[12] << 8));
	volume.sectors_per_cluster = data[13];
	volume.reserved_sectors = uint16_t(data[14] | (data[15] << 8));
	volume.fat_copies = data[16];
	const uint16_t root_directory_entries = uint16_t(data[17] | (data[18] << 8));
	volume.total_sectors = uint16_t(data[19] | (data[20] << 8));
	volume.sectors_per_fat = uint16_t(data[22] | (data[23] << 8));
	volume.sectors_per_track = uint16_t(data[24] | (data[25] << 8));
	volume.head_count = uint16_t(data[26] | (data[27] << 8));
	volume.correct_signature = data[510] == 0x55 && data[511] == 0xaa;

	const size_t root_directory_sectors = (root_directory_entries*32 + volume.bytes_per_sector - 1) / volume.bytes_per_sector;
	volume.first_data_sector = int(volume.reserved_sectors + volume.sectors_per_fat*volume.fat_copies + root_directory_sectors);

	// Grab the FAT.
	std::vector<uint8_t> source_fat;
	for(int c = 0; c < volume.sectors_per_fat; c++) {
		const int sector_number = volume.reserved_sectors + c;
		const auto address = volume.chs_for_sector(sector_number);

		Storage::Encodings::MFM::Sector *const fat_sector =
			parser.get_sector(address.head, address.cylinder, uint8_t(address.sector));
		if(!fat_sector || fat_sector->samples.empty() || fat_sector->samples[0].size() != volume.bytes_per_sector) {
			return std::nullopt;
		}
		std::copy(fat_sector->samples[0].begin(), fat_sector->samples[0].end(), std::back_inserter(source_fat));
	}

	// Decode the FAT.
	// TODO: stop assuming FAT12 here.
	for(size_t c = 0; c < source_fat.size(); c += 3) {
		const uint32_t double_cluster = uint32_t(source_fat[c] + (source_fat[c + 1] << 8) + (source_fat[c + 2] << 16));
		volume.fat.push_back(uint16_t(double_cluster & 0xfff));
		volume.fat.push_back(uint16_t(double_cluster >> 12));
	}

	// Grab the root directory.
	std::vector<uint8_t> root_directory;
	for(size_t c = 0; c < root_directory_sectors; c++) {
		const auto sector_number = int(volume.reserved_sectors + c + volume.sectors_per_fat*volume.fat_copies);
		const auto address = volume.chs_for_sector(sector_number);

		Storage::Encodings::MFM::Sector *const sector =
			parser.get_sector(address.head, address.cylinder, uint8_t(address.sector));
		if(!sector || sector->samples.empty() || sector->samples[0].size() != volume.bytes_per_sector) {
			return std::nullopt;
		}
		std::copy(sector->samples[0].begin(), sector->samples[0].end(), std::back_inserter(root_directory));
	}
	volume.root_directory = directory_from(root_directory);

	return volume;
}

std::optional<std::vector<uint8_t>> FAT::GetFile(const std::shared_ptr<Storage::Disk::Disk> &disk, const Volume &volume, const File &file) {
	Storage::Encodings::MFM::Parser parser(true, disk);

	std::vector<uint8_t> contents;

	// In FAT cluster numbers describe a linked list via the FAT table, with values above $FF0 being reserved
	// (relevantly: FF7 means bad cluster; FF8–FFF mean end-of-file).
	uint16_t cluster = file.starting_cluster;
	do {
		const int sector = volume.sector_for_cluster(cluster);

		for(int c = 0; c < volume.sectors_per_cluster; c++) {
			const auto address = volume.chs_for_sector(sector + c);

			Storage::Encodings::MFM::Sector *const sector_contents =
				parser.get_sector(address.head, address.cylinder, uint8_t(address.sector));
			if(!sector_contents || sector_contents->samples.empty() || sector_contents->samples[0].size() != volume.bytes_per_sector) {
				return std::nullopt;
			}
			std::copy(sector_contents->samples[0].begin(), sector_contents->samples[0].end(), std::back_inserter(contents));
		}

		cluster = volume.fat[cluster];
	} while(cluster < 0xff0);

	return contents;
}

std::optional<FAT::Directory> FAT::GetDirectory(const std::shared_ptr<Storage::Disk::Disk> &disk, const Volume &volume, const File &file) {
	const auto contents = GetFile(disk, volume, file);
	if(!contents) {
		return std::nullopt;
	}
	return directory_from(*contents);
}
