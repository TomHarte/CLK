//
//  PRG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "PRG.hpp"

#include <cstdio>
#include <sys/stat.h>
#include "../Encodings/CommodoreROM.hpp"

using namespace Storage::Cartridge;

PRG::PRG(const char *file_name) {
	struct stat file_stats;
	stat(file_name, &file_stats);

	// accept only files sized less than 8kb
	if(file_stats.st_size > 0x2000 + 2)
		throw ErrorNotROM;

	// get the loading address, and the rest of the contents
	FILE *file = fopen(file_name, "rb");

	int loading_address = fgetc(file);
	loading_address |= fgetc(file) << 8;

	size_t data_length = static_cast<size_t>(file_stats.st_size) - 2;
	size_t padded_data_length = 1;
	while(padded_data_length < data_length) padded_data_length <<= 1;
	std::vector<uint8_t> contents(padded_data_length);
	fread(&contents[0], 1, static_cast<size_t>(data_length), file);
	fclose(file);

	// accept only files intended to load at 0xa000
	if(loading_address != 0xa000)
		throw ErrorNotROM;

	// also accept only cartridges with the proper signature
	if(!Storage::Cartridge::Encodings::CommodoreROM::isROM(contents))
		throw ErrorNotROM;

	segments_.emplace_back(0xa000, 0xa000 + data_length, std::move(contents));
}
