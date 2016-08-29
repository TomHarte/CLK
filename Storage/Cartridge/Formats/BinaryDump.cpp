//
//  BinaryDump.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "BinaryDump.hpp"

#include <cstdio>
#include <sys/stat.h>

using namespace Storage::Cartridge;

BinaryDump::BinaryDump(const char *file_name)
{
	// the file should be exactly 16 kb
	struct stat file_stats;
	stat(file_name, &file_stats);

	// grab contents
	FILE *file = fopen(file_name, "rb");
	if(!file) throw ErrorNotAccessible;
	size_t data_length = (size_t)file_stats.st_size;
	std::vector<uint8_t> contents(data_length);
	fread(&contents[0], 1, (size_t)(data_length), file);
	fclose(file);

	// enshrine
	_segments.emplace_back(
		::Storage::Cartridge::Cartridge::Segment::UnknownAddress,
		::Storage::Cartridge::Cartridge::Segment::UnknownAddress,
		std::move(contents));
}
