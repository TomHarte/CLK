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

using namespace Storage::Cartridge;

PRG::PRG(const char *file_name) : _contents(nullptr)
{
	struct stat file_stats;
	stat(file_name, &file_stats);

	// accept only files sized 1, 2, 4 or 8kb
	if(
		file_stats.st_size != 0x400 + 2 &&
		file_stats.st_size != 0x800 + 2 &&
		file_stats.st_size != 0x1000 + 2 &&
		file_stats.st_size != 0x2000 + 2)
		throw ErrorNotROM;

	// get the loading address, and the rest of the contents
	FILE *file = fopen(file_name, "rb");

	int loading_address = fgetc(file);
	loading_address |= fgetc(file) << 8;

	_contents = new uint8_t[file_stats.st_size - 2];
	fread(_contents, 1, (size_t)(file_stats.st_size - 2), file);
	fclose(file);

	// accept only files intended to load at 0xa000
	if(loading_address != 0xa000)
		throw ErrorNotROM;

	// also accept only cartridges with the proper signature
	if(
		_contents[4] != 0x41 ||
		_contents[5] != 0x30 ||
		_contents[6] != 0xc3 ||
		_contents[7] != 0xc2 ||
		_contents[8] != 0xcd)
		throw ErrorNotROM;
}

PRG::~PRG()
{
	delete[] _contents;
}
