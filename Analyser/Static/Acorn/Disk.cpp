//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"

#include "../../../Storage/Disk/Controller/DiskController.hpp"
#include "../../../Storage/Disk/Encodings/MFM/Parser.hpp"
#include "../../../Numeric/CRC.hpp"

#include <algorithm>

using namespace Analyser::Static::Acorn;

std::unique_ptr<Catalogue> Analyser::Static::Acorn::GetDFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk) {
	// c.f. http://beebwiki.mdfs.net/Acorn_DFS_disc_format
	auto catalogue = std::make_unique<Catalogue>();
	Storage::Encodings::MFM::Parser parser(false, disk);

	const Storage::Encodings::MFM::Sector *const names = parser.get_sector(0, 0, 0);
	const Storage::Encodings::MFM::Sector *const details = parser.get_sector(0, 0, 1);

	if(!names || !details) return nullptr;
	if(names->samples.empty() || details->samples.empty()) return nullptr;
	if(names->samples[0].size() != 256 || details->samples[0].size() != 256) return nullptr;

	uint8_t final_file_offset = details->samples[0][5];
	if(final_file_offset&7) return nullptr;
	if(final_file_offset < 8) return nullptr;

	char disk_name[13];
	snprintf(disk_name, 13, "%.8s%.4s", &names->samples[0][0], &details->samples[0][0]);
	catalogue->name = disk_name;

	switch((details->samples[0][6] >> 4)&3) {
		case 0: catalogue->bootOption = Catalogue::BootOption::None;		break;
		case 1: catalogue->bootOption = Catalogue::BootOption::LoadBOOT;	break;
		case 2: catalogue->bootOption = Catalogue::BootOption::RunBOOT;		break;
		case 3: catalogue->bootOption = Catalogue::BootOption::ExecBOOT;	break;
	}

	for(std::size_t file_offset = 8; file_offset < final_file_offset; file_offset += 8) {
		File new_file;
		char name[10];
		snprintf(name, 10, "%c.%.7s", names->samples[0][file_offset + 7] & 0x7f, &names->samples[0][file_offset]);
		new_file.name = name;
		new_file.load_address = uint32_t(details->samples[0][file_offset] | (details->samples[0][file_offset+1] << 8) | ((details->samples[0][file_offset+6]&0x0c) << 14));
		new_file.execution_address = uint32_t(details->samples[0][file_offset+2] | (details->samples[0][file_offset+3] << 8) | ((details->samples[0][file_offset+6]&0xc0) << 10));
		new_file.is_protected = names->samples[0][file_offset + 7] & 0x80;

		long data_length = long(details->samples[0][file_offset+4] | (details->samples[0][file_offset+5] << 8) | ((details->samples[0][file_offset+6]&0x30) << 12));
		int start_sector = details->samples[0][file_offset+7] | ((details->samples[0][file_offset+6]&0x03) << 8);
		new_file.data.reserve(size_t(data_length));

		if(start_sector < 2) continue;
		while(data_length > 0) {
			uint8_t sector = uint8_t(start_sector % 10);
			uint8_t track = uint8_t(start_sector / 10);
			start_sector++;

			Storage::Encodings::MFM::Sector *next_sector = parser.get_sector(0, track, sector);
			if(!next_sector) break;

			long length_from_sector = std::min(data_length, 256l);
			new_file.data.insert(new_file.data.end(), next_sector->samples[0].begin(), next_sector->samples[0].begin() + length_from_sector);
			data_length -= length_from_sector;
		}
		if(!data_length) catalogue->files.push_back(new_file);
	}

	return catalogue;
}
std::unique_ptr<Catalogue> Analyser::Static::Acorn::GetADFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk) {
	auto catalogue = std::make_unique<Catalogue>();
	Storage::Encodings::MFM::Parser parser(true, disk);

	Storage::Encodings::MFM::Sector *free_space_map_second_half = parser.get_sector(0, 0, 1);
	if(!free_space_map_second_half) return nullptr;

	std::vector<uint8_t> root_directory;
	root_directory.reserve(5 * 256);
	for(uint8_t c = 2; c < 7; c++) {
		const Storage::Encodings::MFM::Sector *const sector = parser.get_sector(0, 0, c);
		if(!sector) return nullptr;
		root_directory.insert(root_directory.end(), sector->samples[0].begin(), sector->samples[0].end());
	}

	// Quick sanity checks.
	if(root_directory[0x4cb]) return nullptr;
	if(root_directory[1] != 'H' || root_directory[2] != 'u' || root_directory[3] != 'g' || root_directory[4] != 'o') return nullptr;
	if(root_directory[0x4FB] != 'H' || root_directory[0x4FC] != 'u' || root_directory[0x4FD] != 'g' || root_directory[0x4FE] != 'o') return nullptr;

	switch(free_space_map_second_half->samples[0][0xfd]) {
		default: catalogue->bootOption = Catalogue::BootOption::None;		break;
		case 1: catalogue->bootOption = Catalogue::BootOption::LoadBOOT;	break;
		case 2: catalogue->bootOption = Catalogue::BootOption::RunBOOT;		break;
		case 3: catalogue->bootOption = Catalogue::BootOption::ExecBOOT;	break;
	}

	return catalogue;
}
