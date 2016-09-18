//
//  SSD.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "SSD.hpp"

#include <sys/stat.h>

using namespace Storage::Disk;

SSD::SSD(const char *file_name) : _file(nullptr)
{
	struct stat file_stats;
	stat(file_name, &file_stats);

	// very loose validation: the file needs to be a multiple of 256 bytes
	// and not ungainly large

	if(file_stats.st_size & 255) throw ErrorNotSSD;
	if(file_stats.st_size < 512) throw ErrorNotSSD;
	if(file_stats.st_size > 800*256) throw ErrorNotSSD;

	_file = fopen(file_name, "rb");

	if(!_file) throw ErrorCantOpen;
}

SSD::~SSD()
{
	if(_file) fclose(_file);
}

unsigned int SSD::get_head_position_count()
{
	return 1;
}

unsigned int SSD::get_head_count()
{
	return 1;
}

std::shared_ptr<Track> SSD::get_track_at_position(unsigned int head, unsigned int position)
{
	std::shared_ptr<Track> track;
	return track;
}
