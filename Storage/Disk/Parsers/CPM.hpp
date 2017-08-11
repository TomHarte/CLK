//
//  CPM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Parsers_CPM_hpp
#define Storage_Disk_Parsers_CPM_hpp

#include "../Disk.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Storage {
namespace Disk {
namespace CPM {

struct ParameterBlock {
	int sectors_per_track;
	int block_size;
	int first_sector;
	int logic_extents_per_physical;
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
}
}

#endif /* Storage_Disk_Parsers_CPM_hpp */
