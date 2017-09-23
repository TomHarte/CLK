//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"
#include "../../Storage/Disk/Controller/DiskController.hpp"
#include "../../Storage/Disk/Encodings/MFM.hpp"
#include "../../NumberTheory/CRC.hpp"
#include <algorithm>

using namespace StaticAnalyser::Acorn;

std::unique_ptr<Catalogue> StaticAnalyser::Acorn::GetDFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk) {
	// c.f. http://beebwiki.mdfs.net/Acorn_DFS_disc_format
	std::unique_ptr<Catalogue> catalogue(new Catalogue);
	Storage::Encodings::MFM::Parser parser(false, disk);

	std::shared_ptr<Storage::Encodings::MFM::Sector> names = parser.get_sector(0, 0, 0);
	std::shared_ptr<Storage::Encodings::MFM::Sector> details = parser.get_sector(0, 0, 1);

	if(!names || !details) return nullptr;
	if(names->data.size() != 256 || details->data.size() != 256) return nullptr;

	uint8_t final_file_offset = details->data[5];
	if(final_file_offset&7) return nullptr;
	if(final_file_offset < 8) return nullptr;

	char disk_name[13];
	snprintf(disk_name, 13, "%.8s%.4s", &names->data[0], &details->data[0]);
	catalogue->name = disk_name;

	switch((details->data[6] >> 4)&3) {
		case 0: catalogue->bootOption = Catalogue::BootOption::None;		break;
		case 1: catalogue->bootOption = Catalogue::BootOption::LoadBOOT;	break;
		case 2: catalogue->bootOption = Catalogue::BootOption::RunBOOT;		break;
		case 3: catalogue->bootOption = Catalogue::BootOption::ExecBOOT;	break;
	}

	// DFS files are stored contiguously, and listed in descending order of distance from track 0.
	// So iterating backwards implies the least amount of seeking.
	for(size_t file_offset = final_file_offset - 8; file_offset > 0; file_offset -= 8) {
		File new_file;
		char name[10];
		snprintf(name, 10, "%c.%.7s", names->data[file_offset + 7] & 0x7f, &names->data[file_offset]);
		new_file.name = name;
		new_file.load_address = (uint32_t)(details->data[file_offset] | (details->data[file_offset+1] << 8) | ((details->data[file_offset+6]&0x0c) << 14));
		new_file.execution_address = (uint32_t)(details->data[file_offset+2] | (details->data[file_offset+3] << 8) | ((details->data[file_offset+6]&0xc0) << 10));
		new_file.is_protected = !!(names->data[file_offset + 7] & 0x80);

		long data_length = (long)(details->data[file_offset+4] | (details->data[file_offset+5] << 8) | ((details->data[file_offset+6]&0x30) << 12));
		int start_sector = details->data[file_offset+7] | ((details->data[file_offset+6]&0x03) << 8);
		new_file.data.reserve((size_t)data_length);

		if(start_sector < 2) continue;
		while(data_length > 0) {
			uint8_t sector = (uint8_t)(start_sector % 10);
			uint8_t track = (uint8_t)(start_sector / 10);
			start_sector++;

			std::shared_ptr<Storage::Encodings::MFM::Sector> next_sector = parser.get_sector(0, track, sector);
			if(!next_sector) break;

			long length_from_sector = std::min(data_length, 256l);
			new_file.data.insert(new_file.data.end(), next_sector->data.begin(), next_sector->data.begin() + length_from_sector);
			data_length -= length_from_sector;
		}
		if(!data_length) catalogue->files.push_front(new_file);
	}

	return catalogue;
}
std::unique_ptr<Catalogue> StaticAnalyser::Acorn::GetADFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk) {
	std::unique_ptr<Catalogue> catalogue(new Catalogue);
	Storage::Encodings::MFM::Parser parser(true, disk);

	std::shared_ptr<Storage::Encodings::MFM::Sector> free_space_map_second_half = parser.get_sector(0, 0, 1);
	if(!free_space_map_second_half) return nullptr;

	std::vector<uint8_t> root_directory;
	root_directory.reserve(5 * 256);
	for(uint8_t c = 2; c < 7; c++) {
		std::shared_ptr<Storage::Encodings::MFM::Sector> sector = parser.get_sector(0, 0, c);
		if(!sector) return nullptr;
		root_directory.insert(root_directory.end(), sector->data.begin(), sector->data.end());
	}

	// Quick sanity checks.
	if(root_directory[0x4cb]) return nullptr;
	if(root_directory[1] != 'H' || root_directory[2] != 'u' || root_directory[3] != 'g' || root_directory[4] != 'o') return nullptr;
	if(root_directory[0x4FB] != 'H' || root_directory[0x4FC] != 'u' || root_directory[0x4FD] != 'g' || root_directory[0x4FE] != 'o') return nullptr;

	switch(free_space_map_second_half->data[0xfd]) {
		default: catalogue->bootOption = Catalogue::BootOption::None;		break;
		case 1: catalogue->bootOption = Catalogue::BootOption::LoadBOOT;	break;
		case 2: catalogue->bootOption = Catalogue::BootOption::RunBOOT;		break;
		case 3: catalogue->bootOption = Catalogue::BootOption::ExecBOOT;	break;
	}

	return catalogue;
}
