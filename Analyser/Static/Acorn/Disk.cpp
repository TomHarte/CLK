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
		if(names->samples[0][file_offset + 7] & 0x80) {
			// File is locked; it may not be altered or deleted.
			new_file.flags |= File::Flags::Locked;
		}

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
		if(!data_length) catalogue->files.push_back(std::move(new_file));
	}

	return catalogue;
}

/*
	Primary resource used: "Acorn 8-Bit ADFS Filesystem Structure";
	http://mdfs.net/Docs/Comp/Disk/Format/ADFS
*/
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

	// Parse the root directory, at least.
	for(std::size_t file_offset = 0x005; file_offset < 0x4cb; file_offset += 0x1a) {
		// Obtain the name, which will be at most ten characters long, and will
		// be terminated by either a NULL character or a \r.
		char name[11];
		std::size_t c = 0;
		for(; c < 10; c++) {
			const char next = root_directory[file_offset + c] & 0x7f;
			name[c] = next;
			if(next == '\0' || next == '\r') break;
		}
		name[c] = '\0';

		// Skip if the name is empty.
		if(!strlen(name)) continue;

		// Populate a file then.
		File new_file;
		new_file.name = name;
		new_file.flags =
			(root_directory[file_offset + 0] & 0x80 ? File::Flags::Readable : 0) |
			(root_directory[file_offset + 1] & 0x80 ? File::Flags::Writable : 0) |
			(root_directory[file_offset + 2] & 0x80 ? File::Flags::Locked : 0) |
			(root_directory[file_offset + 3] & 0x80 ? File::Flags::IsDirectory : 0) |
			(root_directory[file_offset + 4] & 0x80 ? File::Flags::ExecuteOnly : 0) |
			(root_directory[file_offset + 5] & 0x80 ? File::Flags::PubliclyReadable : 0) |
			(root_directory[file_offset + 6] & 0x80 ? File::Flags::PubliclyWritable : 0) |
			(root_directory[file_offset + 7] & 0x80 ? File::Flags::PubliclyExecuteOnly : 0) |
			(root_directory[file_offset + 8] & 0x80 ? File::Flags::IsPrivate : 0);

		new_file.load_address =
			(uint32_t(root_directory[file_offset + 0x0a]) << 0) |
			(uint32_t(root_directory[file_offset + 0x0b]) << 8) |
			(uint32_t(root_directory[file_offset + 0x0c]) << 16) |
			(uint32_t(root_directory[file_offset + 0x0d]) << 24);

		new_file.execution_address =
			(uint32_t(root_directory[file_offset + 0x0e]) << 0) |
			(uint32_t(root_directory[file_offset + 0x0f]) << 8) |
			(uint32_t(root_directory[file_offset + 0x10]) << 16) |
			(uint32_t(root_directory[file_offset + 0x11]) << 24);

		new_file.sequence_number = root_directory[file_offset + 0x19];

		const uint32_t size =
			(uint32_t(root_directory[file_offset + 0x12]) << 0) |
			(uint32_t(root_directory[file_offset + 0x13]) << 8) |
			(uint32_t(root_directory[file_offset + 0x14]) << 16) |
			(uint32_t(root_directory[file_offset + 0x15]) << 24);

		uint32_t start_sector =
			(uint32_t(root_directory[file_offset + 0x16]) << 0) |
			(uint32_t(root_directory[file_offset + 0x17]) << 8) |
			(uint32_t(root_directory[file_offset + 0x18]) << 16);

		new_file.data.reserve(size);
		while(new_file.data.size() < size) {
			const Storage::Encodings::MFM::Sector *const sector = parser.get_sector(start_sector / (80 * 16), (start_sector / 16) % 80, start_sector % 16);
			if(!sector) break;

			const auto length_from_sector = std::min(size - new_file.data.size(), sector->samples[0].size());
			new_file.data.insert(new_file.data.end(), sector->samples[0].begin(), sector->samples[0].begin() + ssize_t(length_from_sector));
			++start_sector;
		}

		catalogue->files.push_back(std::move(new_file));
	}

	return catalogue;
}
