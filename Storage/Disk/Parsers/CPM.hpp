//
//  CPM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Disk.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Storage::Disk::CPM {

struct ParameterBlock {
	int sectors_per_track;
	int tracks;
	int block_size;
	int first_sector;
	uint16_t catalogue_allocation_bitmap;
	int reserved_tracks;

	// Some well-known formats.
	static ParameterBlock cpc_data_format() {
		Storage::Disk::CPM::ParameterBlock data_format;
		data_format.sectors_per_track = 9;
		data_format.tracks = 40;
		data_format.block_size = 1024;
		data_format.first_sector = 0xc1;
		data_format.catalogue_allocation_bitmap = 0xc000;
		data_format.reserved_tracks = 0;
		return data_format;
	}

	static ParameterBlock cpc_system_format() {
		Storage::Disk::CPM::ParameterBlock system_format;
		system_format.sectors_per_track = 9;
		system_format.tracks = 40;
		system_format.block_size = 1024;
		system_format.first_sector = 0x41;
		system_format.catalogue_allocation_bitmap = 0xc000;
		system_format.reserved_tracks = 2;
		return system_format;
	}
};

struct File {
	uint8_t user_number;
	std::string name;
	std::string type;
	bool read_only;
	bool system;
	std::vector<uint8_t> data;
};

struct Catalogue {
	std::vector<File> files;

	bool is_zx_spectrum_booter();
};

std::unique_ptr<Catalogue> GetCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk, const ParameterBlock &parameters);

}
