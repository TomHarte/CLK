//
//  FAT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Parsers_FAT_hpp
#define Storage_Disk_Parsers_FAT_hpp

#include "../Disk.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Storage {
namespace Disk {
namespace FAT {

struct File {
	std::string name;
	std::string extension;
	uint8_t attributes = 0;
	uint16_t time = 0;	// TODO: offer time/date decoders.
	uint16_t date = 0;
	uint16_t starting_cluster = 0;
	uint32_t size = 0;

	enum Attribute: uint8_t {
		ReadOnly	= (1 << 0),
		Hidden		= (1 << 1),
		System		= (1 << 2),
		VolumeLabel	= (1 << 3),
		Directory	= (1 << 4),
		Archive		= (1 << 5),
	};
};

using Directory = std::vector<File>;

struct Volume {
	uint16_t bytes_per_sector = 0;
	uint8_t sectors_per_cluster = 0;
	uint16_t reserved_sectors = 0;
	uint8_t fat_copies = 0;
	uint16_t total_sectors = 0;
	uint16_t sectors_per_fat = 0;
	uint16_t sectors_per_track = 0;
	uint16_t head_count = 0;
	uint16_t hidden_sectors = 0;
	bool correct_signature = false;
	int first_data_sector = 0;

	std::vector<uint16_t> fat;
	Directory root_directory;

	struct CHS {
		int cylinder;
		int head;
		int sector;
	};
	/// @returns a direct sector -> CHS address translation.
	CHS chs_for_sector(int sector) const;
	/// @returns the CHS address for the numbered cluster within the data area.
	int sector_for_cluster(uint16_t cluster) const;
};

std::optional<Volume> GetVolume(const std::shared_ptr<Storage::Disk::Disk> &disk);
std::optional<std::vector<uint8_t>> GetFile(const std::shared_ptr<Storage::Disk::Disk> &disk, const Volume &volume, const File &file);
std::optional<Directory> GetDirectory(const std::shared_ptr<Storage::Disk::Disk> &disk, const Volume &volume, const File &file);

}
}
}

#endif /* FAT_hpp */
