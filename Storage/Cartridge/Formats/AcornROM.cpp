//
//  AcornROM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 28/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AcornROM.hpp"

#include <cstdio>
#include <sys/stat.h>

using namespace Storage::Cartridge;

AcornROM::AcornROM(const char *file_name)
{
	// the file should be exactly 16 kb
	struct stat file_stats;
	stat(file_name, &file_stats);
	if(file_stats.st_size != 0x4000) throw ErrorNotAcornROM;

	// grab contents
	FILE *file = fopen(file_name, "rb");
	if(!file) throw ErrorNotAcornROM;
	size_t data_length = (size_t)file_stats.st_size;
	std::vector<uint8_t> contents(data_length);
	fread(&contents[0], 1, (size_t)(data_length), file);
	fclose(file);

	// perform sanity checks...

	// is a copyright string present?
	uint8_t copyright_offset = contents[7];
	if(
		contents[copyright_offset] != 0x00 ||
		contents[copyright_offset+1] != 0x28 ||
		contents[copyright_offset+2] != 0x43 ||
		contents[copyright_offset+3] != 0x29
	) throw ErrorNotAcornROM;

	// is the language entry point valid?
	if(!(
		(contents[0] == 0x00 && contents[1] == 0x00 && contents[2] == 0x00) ||
		(contents[0] != 0x00 && contents[2] >= 0x80 && contents[2] < 0xc0)
		)) throw ErrorNotAcornROM;

	// is the service entry point valid?
	if(!(contents[5] >= 0x80 && contents[5] < 0xc0)) throw ErrorNotAcornROM;

	// probability of a random binary blob that isn't an Acorn ROM proceeding to here:
	//		1/(2^32) *
	//		( ((2^24)-1)/(2^24)*(1/4)		+		1/(2^24)	) *
	//		1/4
	//	= something very improbable — around 1/16th of 1 in 2^32, but not exactly.

	// enshrine
	_segments.emplace_back(0x8000, 0xc000, std::move(contents));
}
