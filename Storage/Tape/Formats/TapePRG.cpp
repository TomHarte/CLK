//
//  TapePRG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/08/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "TapePRG.hpp"

#include <sys/stat.h>

using namespace Storage;

TapePRG::TapePRG(const char *file_name) : _file(nullptr)
{
	struct stat file_stats;
	stat(file_name, &file_stats);

	// There's really no way to validate other than that if this file is larger than 64kb,
	// of if load address + length > 65536 then it's broken.
	if(file_stats.st_size >= 65538 || file_stats.st_size < 3)
		throw ErrorBadFormat;

	_file = fopen(file_name, "rb");
	if(!_file) throw ErrorBadFormat;

	_load_address = (uint16_t)fgetc(_file);
	_load_address |= (uint16_t)fgetc(_file) << 8;

	if (_load_address + file_stats.st_size >= 65536)
		throw ErrorBadFormat;
}

TapePRG::~TapePRG()
{
	if(_file) fclose(_file);
}

Tape::Pulse TapePRG::get_next_pulse()
{
	Tape::Pulse pulse;
	return pulse;
}

void TapePRG::reset()
{
	// TODO
}
