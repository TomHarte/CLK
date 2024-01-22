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
};

std::unique_ptr<Catalogue> GetCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk, const ParameterBlock &parameters);

}
